/***************************************************************************
 *   Copyright (C) 2020 by Kyle Hayes                                      *
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
#include "plc.h"
#include "buf.h"
#include "socket.h"
#include "tcp_server.h"
#include "utils.h"



static void reset_buffers(tcp_client_p client)
{
    info("Resetting buffers.");

    /* FIXME - this needs to include the header overhead for the size! */
    client->request = client->buffer;
    buf_set_cap(&(client->request), client->conn_config.client_to_server_max_packet + 64); /* MAGIC */
    buf_set_start(&(client->request), 0);
    buf_set_cursor(&(client->request), 0);

    client->response = client->buffer;
    buf_set_cap(&(client->response), client->conn_config.server_to_client_max_packet);
    buf_set_start(&(client->response), 0);
    buf_set_end(&(client->response), buf_cap(&(client->response)));
    buf_set_cursor(&(client->response), 0);
}



static THREAD_FUNC(tcp_client_connection_handler, raw_client_ptr)
{
    int rc;
    tcp_client_p client = (tcp_client_p)raw_client_ptr;
    bool done = 0;

    reset_buffers(client);

    info("Got new client connection, going into processing loop.");
    do {
        rc = TCP_CLIENT_PROCESSED;

        /* get an incoming packet or a partial packet. */
        rc = socket_read(client->sock_fd, &(client->request));

        if(rc < 0) {
            info("WARN: error response reading socket! error %d", rc);
            perror("SOCKET READ ERROR");
            rc = TCP_CLIENT_DONE;
            break;
        }

        /* try to process the packet. */
        rc = client->handler(client);

        /* check the response. */
        if(rc == TCP_CLIENT_PROCESSED) {
            /* FIXME - this should be in a loop to make sure all data is pushed. */
            rc = socket_write(client->sock_fd, &(client->response));

            /* error writing? */
            if(rc < 0) {
                info("ERROR: error writing output packet! Error: %d", rc);
                perror("SOCKET WRITE ERROR:");
                rc = TCP_CLIENT_DONE;
                done = true;
                break;
            } else {
                /* all good. Reset the buffers etc. */

                reset_buffers(client);
            }
        } else {
            /* there was some sort of error or exceptional condition. */
            switch(rc) {
                case TCP_CLIENT_DONE:
                    info("Client is done.");
                    done = true;
                    break;

                case TCP_CLIENT_PROCESSED:
                    break;

                case TCP_CLIENT_INCOMPLETE:
                    // /* set the cursor to the end of the buffer data */
                    // info("Setting buffer cursor back to %u.", buf_len(&(client->request)));
                    // buf_set_cursor(&(client->request), buf_len(&(client->request)));

                    // /* return the full length back to what it was. */
                    // info("Setting buffer length back to %u.", old_length);
                    // buf_set_len(&(client->request), old_length);
                    break;

                case TCP_CLIENT_UNSUPPORTED:
                    info("WARN: Unsupported request!");
                    buf_dump(&(client->request));
                    break;

                default:
                    info("WARN: Unsupported return code %d!", rc);
                    break;
            }
        }

        if(client->terminate) {
            info("INFO: Termination flag set.  Closing down.");
            rc = TCP_CLIENT_DONE;
        }
    } while(rc == TCP_CLIENT_INCOMPLETE || rc == TCP_CLIENT_PROCESSED);

    /* done with the socket */
    socket_close(client->sock_fd);

    free(client);

    THREAD_RETURN(0);
}


// void tcp_server_run(const char *host, const char *port, int (*handler)(tcp_client_p client), size_t buffer_size, plc_s *plc, volatile sig_atomic_t *terminate, void *context);
void tcp_server_run(const char *host, const char *port, int (*handler)(tcp_client_p client), size_t buffer_size, plc_s *plc, volatile sig_atomic_t *terminate, void *context)
{
    tcp_client_p client = NULL;
    int listen_fd;
    int client_fd;

    listen_fd = socket_open(host, port);

    if(listen_fd < 0) {
        error("ERROR: Unable to open listener TCP socket, error code!");
    }

    do {
        //info("Waiting for new client connection.");

        /* FIXME - use select or something similar here to wait for something to do. */
        client_fd = socket_accept(listen_fd);

        if(client_fd >= 0) {
            tcp_client_p tcp_client = calloc(1, sizeof(*tcp_client) + buffer_size);
            thread_t client_thread;

            if(!tcp_client) {
                error("ERROR: Unable to allocate space for new client connection!");
            }

            tcp_client->sock_fd = client_fd;
            tcp_client->buffer = buf_make(&(tcp_client->buffer_data[0]), buffer_size);
            tcp_client->handler = handler;
            tcp_client->terminate = terminate;
            tcp_client->plc = plc;
            tcp_client->context = context;

            /* copy over all the default connection config */
            tcp_client->conn_config = plc->default_conn_config;

            /* set up the request buffer */
            tcp_client->request = tcp_client->buffer;
            buf_set_cap(&(tcp_client->request), tcp_client->conn_config.client_to_server_max_packet);

            /* set up the response buffer */
            tcp_client->response = tcp_client->buffer;
            buf_set_cap(&(tcp_client->response), tcp_client->conn_config.server_to_client_max_packet);

            /* create the thread for the connection */
            info("Creating thread to handle the new connection.");
            if(thread_create(&client_thread, tcp_client_connection_handler, (thread_arg_t)tcp_client)) {
                error("ERROR: Unable to create client connection handler thread!");
            }
        } else if (client_fd != SOCKET_STATUS_OK) {
            /* There was an error either opening or accepting! */
            error("ERROR: error while trying to open/accept the client socket.");
        }

        /* wait a bit to give back the CPU. */
        util_sleep_ms(5);
    } while(!*terminate);

    socket_close(listen_fd);

    /* tacky.   Wait for threads to stop.  */
    util_sleep_ms(100);

    return;
}





// void tcp_server_destroy(tcp_server_p server)
// {
//     if(server) {
//         if(server->sock_fd >= 0) {
//             socket_close(server->sock_fd);
//             server->sock_fd = INT_MIN;
//         }
//         free(server);
//     }
// }
