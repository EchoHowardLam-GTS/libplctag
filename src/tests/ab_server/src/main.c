/***************************************************************************
 *   Copyright (C) 2024 by Kyle Hayes                                      *
 *   Author Kyle Hayes  kyle.hayes@gmail.com                               *
 *                                                                         *
 * This software is available under either the Mozilla Public License      *
 * version 2.0 or the GNU LGPL version 2 (or later) license, whichever     *
 * you choose.                                                             *
 *                                                                         *
 * MPL 2.0:                                                                *
 *                                                                         *
 *   This Source Code Form is subject to the terms of the Mozilla Public   *
 *   License, v. 2.0. If a copy of the MPL was not distributed with this   *
 *   file, You can obtain one at http://mozilla.org/MPL/2.0/.              *
 *                                                                         *
 *                                                                         *
 * LGPL 2:                                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "utils/compat.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(IS_WINDOWS)
#include <Windows.h>
#else
 /* assume it is POSIX of some sort... */
#include <signal.h>
#include <strings.h>
#endif

#include "utils/debug.h"
#include "eip.h"
#include "plc.h"
#include "utils/slice.h"
#include "utils/tcp_server.h"
#include "utils/time_utils.h"

/*
 * build on plc_connection_config
 */
typedef struct {
    tcp_client_t;
    struct plc_connection_config conn_config;
    plc_s *plc;
} client_state_t;

typedef client_state_t *client_state_p;

static void usage(void);
// static void process_args(int argc, const char **argv, plc_s *plc);
// static void parse_path(const char *path, plc_s *plc);
// static void parse_pccc_tag(const char *tag, plc_s *plc);
// static void parse_cip_tag(const char *tag, plc_s *plc);
static int request_handler(tcp_client_p client);


#ifdef IS_WINDOWS

typedef volatile int sig_flag_t;

sig_flag_t done = 0;

/* straight from MS' web site :-) */
int WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
        info("^C event");
        done = 1;
        return TRUE;

        // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
        info("Close event");
        done = 1;
        return TRUE;

        // Pass other signals to the next handler.
    case CTRL_BREAK_EVENT:
        info("^Break event");
        done = 1;
        return TRUE;

    case CTRL_LOGOFF_EVENT:
        info("Logoff event");
        done = 1;
        return TRUE;

    case CTRL_SHUTDOWN_EVENT:
        info("Shutdown event");
        done = 1;
        return TRUE;

    default:
        info("Default Event: %d", fdwCtrlType);
        return FALSE;
    }
}


void setup_break_handler(void)
{
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE))
    {
        printf("\nERROR: Could not set control handler!\n");
        usage();
    }
}

#else

typedef volatile sig_atomic_t sig_flag_t;

sig_flag_t done = 0;

void SIGINT_handler(int not_used)
{
    (void)not_used;

    done = 1;
}

void setup_break_handler(void)
{
    struct sigaction act;

    /* set up signal handler. */
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIGINT_handler;
    sigaction(SIGINT, &act, NULL);
}

#endif


int main(int argc, const char **argv)
{
    plc_s plc;
    size_t buffer_size = 8192;

    /* set up handler for ^C etc. */
    setup_break_handler();

    debug_set_level(DEBUG_INFO);

    /* clear out context to make sure we do not get gremlins */
    memset(&plc, 0, sizeof(plc));

    /* set the random seed. */
    srand((unsigned int)time(NULL));

    process_args(argc, argv, &plc);

    /* open a server connection and listen on the right port. */
    tcp_server_run("0.0.0.0", (plc.port_str ? plc.port_str : "44818"), request_handler, buffer_size, &plc, &done, NULL);

    return 0;
}


void usage(void)
{
    fprintf(stderr, "Usage: ab_server --plc=<plc_type> [--path=<path>] [--port=<port>] --tag=<tag>\n"
                    "   <plc type> = one of the CIP PLCs: \"ControlLogix\", \"Micro800\" or \"Omron\",\n"
                    "                or one of the PCCC PLCs: \"PLC/5\", \"SLC500\" or \"Micrologix\".\n"
                    "\n"
                    "   <path> = (required for ControlLogix) internal path to CPU in PLC.  E.g. \"1,0\".\n"
                    "\n"
                    "   <port> = (required for ControlLogix) internal path to CPU in PLC.  E.g. \"1,0\".\n"
                    "            Defaults to 44818.\n"
                    "\n"
                    "    PCCC-based PLC tags are in the format: <file>[<size>] where:\n"
                    "        <file> is the data file, only the following are supported:\n"
                    "            N7   - 2-byte signed integer.\n"
                    "            F8   - 4-byte floating point number.\n"
                    "            ST18 - 82-byte ASCII string.\n"
                    "            L19  - 4-byte signed integer.\n"
                    "\n"
                    "        <size> field is the length of the data file.\n"
                    "\n"
                    "    CIP-based PLC tags are in the format: <name>:<type>[<sizes>] where:\n"
                    "        <name> is alphanumeric, starting with an alpha character.\n"
                    "        <type> is one of:\n"
                    "            SINT   - 1-byte signed integer.  Requires array size(s).\n"
                    "            INT    - 2-byte signed integer.  Requires array size(s).\n"
                    "            DINT   - 4-byte signed integer.  Requires array size(s).\n"
                    "            LINT   - 8-byte signed integer.  Requires array size(s).\n"
                    "            REAL   - 4-byte floating point number.  Requires array size(s).\n"
                    "            LREAL  - 8-byte floating point number.  Requires array size(s).\n"
                    "            STRING - 82-byte string.  Requires array size(s).\n"
                    "            BOOL   - 1-byte boolean value.  Requires array size(s).\n"
                    "\n"
                    "        <sizes> field is one or more (up to 3) numbers separated by commas.\n"
                    "\n"
                    "Example: ab_server --plc=ControlLogix --path=1,0 --tag=MyTag:DINT[10,10]\n");

    exit(1);
}


tcp_client_p allocate_client(void *plc_arg)
{
    plc_s *plc = (plc_s *)plc_arg;
    client_state_p client = NULL;

    size_t client_state_size = sizeof(*client) + plc->default_conn_config.default_buffer_size;

    client = calloc(1, client_state_size);

    error_assert((client), "Unable to allocate new client state!");

    client->buffer.start = (uint8_t *)(client + 1);
    client->buffer.end = client->buffer.start + plc->default_conn_config.default_buffer_size;
    client->handler = request_handler;
    client->conn_config = plc->default_conn_config;
    client->plc = plc;


    return client;
}

/*
 * Process each request.  Dispatch to the correct
 * request type handler.
 */

tcp_client_status_t request_handler(slice_p request, slice_p response, tcp_client_p client_arg)
{
    int rc = TCP_CLIENT_DONE;
    client_state_p client = (client_state_p)client_arg;

    /* dispatch the data */
    rc = eip_dispatch_request(client);

    /* if there is a response delay requested, then wait a bit. */
    if(rc == TCP_CLIENT_PROCESSED) {
        if(client->conn_config.response_delay > 0) {
            util_sleep_ms(client->conn_config.response_delay);
        }
    }

    /* we do not have a complete packet, get more data. */
    return rc;
}
