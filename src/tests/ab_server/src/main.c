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

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eip.h"
#include "plc.h"
#include "arg_parser.h"
#include "utils/compat.h"
#include "utils/debug.h"
#include "utils/slice.h"
#include "utils/status.h"
#include "utils/tcp_server.h"
#include "utils/time_utils.h"

static plc_connection_t template_plc_connection = {0};

static void usage(void);
static tcp_connection_p allocate_client(void *template_connection_arg);


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
    /* set up handler for ^C etc. */
    setup_break_handler();

    debug_set_level(DEBUG_INFO);

    /* set the random seed. */
    srand((unsigned int)util_time_ms());

    if(!process_args(argc, argv, &template_plc_connection)) {
        usage();
    }

    /* open a server connection and listen on the right port. */
    tcp_server_run("0.0.0.0", (template_plc_connection.port_string ? template_plc_connection.port_string : "44818"), &done, allocate_client, (void *)&template_plc_connection);

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


tcp_connection_p allocate_client(void *template_connection_arg)
{
    plc_connection_p template_connection = (plc_connection_p)template_connection_arg;
    size_t connection_state_size = sizeof(*template_connection);
    plc_connection_p connection = NULL;

    connection = calloc(1, connection_state_size);

    assert_error((connection), "Unable to allocate new connection state!");

    /* copy the template data */
    *connection = *template_connection;

    /* fill in anything that changes */
    connection->tcp_connection.request_buffer.start = &(connection->pdu_data_buffer);
    connection->tcp_connection.request_buffer.end = &(connection->pdu_data_buffer) + MAX_DEVICE_BUFFER_SIZE;
    connection->tcp_connection.response_buffer.start = &(connection->pdu_data_buffer);
    connection->tcp_connection.response_buffer.end = &(connection->pdu_data_buffer) + MAX_DEVICE_BUFFER_SIZE;
    connection->tcp_connection.handler = eip_dispatch_request;

    return (tcp_connection_p)connection;
}
