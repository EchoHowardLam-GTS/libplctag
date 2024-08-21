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

    /* initial PDU */
    pdu_t buffer;
};

typedef struct tcp_connection_t tcp_connection_t;

typedef tcp_connection_t *tcp_connection_p;



// static tcp_connection_p allocate_new_client(void *app_data);
static THREAD_FUNC(connection_handler, connection_ptr_arg);

tcp_server_run(tcp_server_config_p config)
{
    status_t rc = STATUS_OK;
    int listen_fd;
    int client_fd;
    tcp_connection_p connection = NULL;
    thread_t client_thread;
    uint32_t client_state_size = sizeof(tcp_connection_t) + config->app_connection_data_size + config->buffer_size;
    uint8_t *data_buffer = NULL;

    rc = socket_open(config->host, config->port, true, &listen_fd);

    assert_error((rc == STATUS_OK), "Unable to open listener TCP socket, error code!");

    do {
        flood("Waiting for new client connections.");
        rc = socket_accept(listen_fd, &client_fd, 200);

        if(rc == STATUS_OK) {
            info("Allocating new TCP client.");
            connection = calloc(1, client_state_size);

            assert_error((connection), "Unable to allocate memory for new client connection!");

            if(!connection) {
                warn("Unable to allocate memory for new client connection!");
                break;
            }

            /* determine the pointers for the app connection data and the buffer into the large block of memory */
            data_buffer = (uint8_t*)connection + sizeof(tcp_connection_t) + config->app_connection_data_size;

            /* initialize the fields of the connection */
            connection->sock_fd = client_fd;
            connection->server_config = config;
            connection->app_data = config->app_data;
            connection->app_connection_data = (app_connection_data_p)((char*)connection + sizeof(tcp_connection_t));
            connection->buffer.request.start = data_buffer;
            connection->buffer.request.end = data_buffer + config->buffer_size;
            connection->buffer.response.start = data_buffer;
            connection->buffer.response.end = data_buffer + config->buffer_size;

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
        } /* else any other value than timeout and we'll drop out of the loop */
    } while(!(config->program_terminating(config->app_data)) && (rc == STATUS_OK || rc == STATUS_TIMEOUT));

    config->terminate_program(config->app_data);

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
    slice_t request = {0};
    slice_t response = {0};
    slice_t remaining_buffer = {0};
    pdu_t pdu = {0};

    info("Got new client connection, going into processing loop.");
    do {
        rc = STATUS_OK;

        /* reset the buffers */
        detail("Resetting request and response buffers.");
        request = connection->buffer.request;
        response = connection->buffer.response;
        remaining_buffer = request;

        do {
            /* get an incoming packet or a partial packet. */
            rc = socket_read(connection->sock_fd, &remaining_buffer, 100);

            /* did we get data? */
            if(rc == STATUS_OK) {
                /* set the request end to the end of the new data */
                if((rc = slice_truncate_to_ptr(&request, remaining_buffer.start)) != STATUS_OK) {
                    warn("Error %s trying to truncate request slice!", status_to_str(rc));
                    break;
                }

                /* try to process the packet. */
                pdu.request = request;
                pdu.response = response;
                rc = connection->server_config->process_request(&pdu, connection->app_connection_data, connection->app_data);
            }
        } while(!connection->server_config->program_terminating(connection->app_data) && (rc == STATUS_OK || rc == STATUS_TIMEOUT));

        /* check the response. */
        if(conn_rc == STATUS_OK) {
            /* write out the response */
            do {
                rc = socket_write(connection->sock_fd, &response, 100);
            } while(!*(connection->terminate)
                    && (rc == STATUS_OK || rc == STATUS_TIMEOUT)
                    && slice_get_len(&response) > 0
                   );

            if(slice_get_len(&response)) {
                /* if we could not write the response, kill the connection. */
                info("Unable to write full response!");
                conn_rc = STATUS_TERMINATE;
                break;
            }
        }
    } while(!*(connection->terminate) && conn_rc == STATUS_OK);

    info("TCP client connection thread is terminating.");

    /* done with the socket */
    socket_close(connection->sock_fd);

    free(connection);

    THREAD_RETURN(0);
}
