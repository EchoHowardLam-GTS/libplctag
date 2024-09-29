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
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include "compat.h"
#include "debug.h"
#include "slice.h"
#include "socket.h"
#include "tcp_server.h"
#include "thread_compat.h"
#include "time_utils.h"




struct tcp_connection_t {
    /* client socket */
    SOCKET sock_fd;

    tcp_server_config_p server_config;
    app_data_p app_data;
    app_connection_data_p app_connection_data;

    slice_t buffer;
};

typedef struct tcp_connection_t tcp_connection_t;

typedef tcp_connection_t *tcp_connection_p;


static volatile bool server_running = false;


// static tcp_connection_p allocate_new_client(void *app_data);
static THREAD_FUNC(connection_handler, connection_ptr_arg);

void tcp_server_run(tcp_server_config_p config)
{
    status_t rc = STATUS_OK;
    int listen_fd;
    int client_fd;
    tcp_connection_p connection = NULL;
    thread_t client_thread;
    uint32_t client_state_size = sizeof(tcp_connection_t) + config->app_connection_data_size + config->buffer_size;
    uint8_t *data_buffer = NULL;

    /* starting up, set the run control flag */
    server_running = true;

    rc = socket_open(config->host, config->port, true, &listen_fd);

    assert_error((rc == STATUS_OK), "Unable to open listener TCP socket, error code %s!", status_to_str(rc));

    do {
        socket_event_t events = SOCKET_EVENT_NONE;

        flood("Waiting for new client connections.");

        do {
            rc = socket_event_wait(listen_fd, SOCKET_EVENT_ACCEPT, &events, 150);
            if(rc != STATUS_OK) {
                /* punt and let the main loop do the checking and rerun.*/
                break;
            }

            if(!(events & SOCKET_EVENT_ACCEPT)) {
                /* not our event? */
                break;
            }

                /* we got an event that accept can be run. */
            rc = socket_accept(listen_fd, &client_fd);
            if(rc != STATUS_OK) {
                warn("Unable to accept new client connection!  Got error %s!", status_to_str(rc));
                break;
            }

            info("Allocating new TCP client.");
            connection = calloc(1, client_state_size);
            if(!connection) {
                warn("Unable to allocate memory for new client connection!");
                rc = STATUS_NO_RESOURCE;
                break;
            }

            /* determine the pointers for the app connection data and the buffer into the large block of memory */
            data_buffer = (uint8_t*)connection + sizeof(tcp_connection_t) + config->app_connection_data_size;

            /* initialize the fields of the connection */
            connection->sock_fd = client_fd;
            connection->server_config = config;
            connection->app_data = config->app_data;
            connection->app_connection_data = (app_connection_data_p)((char*)connection + sizeof(tcp_connection_t));

            if(!slice_init_parent(&(connection->buffer), data_buffer, config->buffer_size)) {
                rc = slice_get_status(&(connection->buffer));
                warn("Unable to initialize the buffer slice, with error %s!", status_to_str(rc));
                break;
            }

            /* call the app-provided connection state init function to set up the state */
            rc = config->init_app_connection_data(connection->app_connection_data, connection->app_data);
            if(rc != STATUS_OK) {
                warn("Error %s trying to initialize application connection data!", status_to_str(rc));
                break;
            }

            /* create the thread for the connection */
            info("Creating thread to handle the new connection.");
            thread_create(&client_thread, connection_handler, (thread_arg_t)connection);

            assert_error((client_thread), "Unable to create client connection handler thread!");
        } while(0); /* else any other value than timeout and we'll drop out of the loop */

        if(config->program_terminating(config->app_data)) {
            warn("App is flagged for termination.");
            server_running = false;
        }
    } while(server_running && (rc == STATUS_OK || rc == STATUS_TIMEOUT));

    info("TCP server run function quitting.");

    socket_close(listen_fd);

    /*
     * Wait for threads to stop.  We don't really need to do this as everything will be
     * cleaned up when the process quits.
     */
    util_sleep_ms(500);

    return;
}



/******** Helpers *********/




THREAD_FUNC(connection_handler, connection_ptr_arg)
{
    status_t rc = STATUS_OK;
    tcp_connection_p connection = (tcp_connection_p)connection_ptr_arg;
    slice_t pdu = {0};

    info("Got new client connection, going into processing loop.");
    do {
        rc = STATUS_OK;

        /* reset the buffers */
        detail("Resetting request and response buffers.");

        /* reinitialize the PDU slice as a child of the main buffer. */
        if(!slice_init_child(&pdu, &(connection->buffer))) {
            rc = slice_get_status(&pdu);
            if(rc != STATUS_OK) {
                warn("Unable to initialize the PDU buffer, error %s!", status_to_str(rc));
                break;
            }
        }

        /* reset the length to zero. */
        if(!slice_set_len(&pdu, 0)) {
            rc = slice_get_status(&pdu);
            if(rc != STATUS_OK) {
                warn("Unable to set the PDU buffer length, error %s!", status_to_str(rc));
                break;
            }
        }

        /* process one request */
        do {
            /* Loop until we have the requested amount of data. */
            do {
                socket_event_t events = SOCKET_EVENT_NONE;

                rc = socket_event_wait(connection->sock_fd, SOCKET_EVENT_READ, &events, 100);
                if(rc == STATUS_OK) {
                    /* read the socket, returns partial if we did not real the whole buffer. */
                    rc = socket_read(connection->sock_fd, &pdu);
                }
            } while(server_running && (rc == STATUS_TIMEOUT || rc == STATUS_PARTIAL));


            /* we read the requested amount of data for a PDU request, now process it. */
            if(rc == STATUS_OK) {
                uint32_t pdu_len = 0;

                /* ready the PDU for processing. */
                rc = slice_set_start(&pdu, 0);
                if(rc != STATUS_OK) {
                    warn("Error trying to set the slice start index!");
                    break;
                }

                pdu_len = slice_get_len(&pdu);
                if(pdu_len == SLICE_LEN_ERROR) {
                    warn("Error getting the PDU slice length!");
                    break;
                }

                if(pdu_len > 0) {
                    info("Got request PDU:");
                    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(&pdu), slice_get_end_ptr(&pdu));
                } else {
                    info("Got zero length request PDU.");
                }

                rc = connection->server_config->process_request(&pdu, connection->app_connection_data, connection->app_data);
            }

            if(rc == STATUS_OK) {
                /* we processed the PDU, now write the results */

                info("Ready to write PDU response:");
                debug_dump_ptr(DEBUG_INFO, pdu.data + pdu.start, pdu.data + pdu.end);

                /* Loop until we write the requested amount of data. */
                do {
                    socket_event_t events = SOCKET_EVENT_NONE;

                    rc = socket_event_wait(connection->sock_fd, SOCKET_EVENT_WRITE, &events, 100);
                    if(rc == STATUS_OK) {
                        /* read the socket, returns partial if we did not write the whole buffer. */
                        rc = socket_write(connection->sock_fd, &pdu);
                    }
                } while(server_running && (rc == STATUS_TIMEOUT || rc == STATUS_PARTIAL)); /* write loop */
            }
        } while(server_running && (rc == STATUS_TIMEOUT || rc == STATUS_PARTIAL)); /* single request retry loop */
    } while(server_running && rc == STATUS_OK); /* main processing loop */

    info("TCP client connection thread is terminating.");

    /* done with the socket */
    socket_close(connection->sock_fd);

    free(connection);

    THREAD_RETURN(0);
}
