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


// static tcp_connection_p allocate_new_client(void *app_data);
static THREAD_FUNC(tcp_client_connection_handler, connection_ptr_arg);


void tcp_server_run(const char *host, const char *port, volatile sig_atomic_t *terminate, tcp_connection_allocate_func allocator, void *app_data)
{
    status_t sock_rc = STATUS_OK;
    int listen_fd;
    int client_fd;
    tcp_connection_p tcp_client = NULL;
    thread_t client_thread;

    sock_rc = socket_open(host, port, &listen_fd);

    assert_error((sock_rc == STATUS_OK), "Unable to open listener TCP socket, error code!");

    do {
        flood("Waiting for new client connections.");
        sock_rc = socket_accept(listen_fd, &client_fd, 200);

        if(sock_rc == STATUS_OK) {
            info("Allocating new TCP client.");
            tcp_client = allocator(app_data);

            assert_error((tcp_client), "Unable to allocate memory for new client connection!");

            tcp_client->sock_fd = client_fd;
            tcp_client->terminate = terminate;

            /* create the thread for the connection */
            info("Creating thread to handle the new connection.");
            thread_create(&client_thread, tcp_client_connection_handler, (thread_arg_t)tcp_client);

            assert_error((client_thread), "Unable to create client connection handler thread!");
        } /* else any other value than timeout and we'll drop out of the loop */
    } while(!*terminate && (sock_rc == STATUS_OK || sock_rc == STATUS_ERR_TIMEOUT));

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


THREAD_FUNC(tcp_client_connection_handler, connection_ptr_arg)
{
    status_t conn_rc = STATUS_OK;
    status_t sock_rc = STATUS_OK;
    tcp_connection_p connection = (tcp_connection_p)connection_ptr_arg;
    slice_t request = {0};
    slice_t response = {0};
    slice_t remaining_buffer = {0};

    info("Got new client connection, going into processing loop.");
    do {
        conn_rc = STATUS_OK;
        sock_rc = STATUS_OK;

        /* reset the buffers */
        detail("Resetting request and response buffers.");
        request = connection->request_buffer;
        response = connection->response_buffer;
        remaining_buffer = request;

        do {
            /* get an incoming packet or a partial packet. */
            sock_rc = socket_read(connection->sock_fd, &remaining_buffer, 100);

            /* did we get data? */
            if(sock_rc == STATUS_OK) {
                /* set the request end to the end of the new data */
                if(!slice_truncate_to_ptr(&request, remaining_buffer.start)) {
                    warn("Unable to truncate request slice!");
                    conn_rc = STATUS_ERR_OP_FAILED;
                    break;
                }

                /* try to process the packet. */
                conn_rc = connection->handler(&request, &response, connection);
            }
        } while(!*(connection->terminate)
                && (sock_rc == STATUS_OK || sock_rc == STATUS_ERR_TIMEOUT)
                && conn_rc == STATUS_ERR_RESOURCE
               );

        /* check the response. */
        if(conn_rc == STATUS_OK) {
            /* write out the response */
            do {
                sock_rc = socket_write(connection->sock_fd, &response, 100);
            } while(!*(connection->terminate)
                    && (sock_rc == STATUS_OK || sock_rc == STATUS_ERR_TIMEOUT)
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
