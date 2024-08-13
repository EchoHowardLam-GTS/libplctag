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

#pragma once

#include <signal.h>
#include "slice.h"
#include "socket.h"


typedef enum {
    /* TCP connection set ready/has no error. */
    TCP_CONNECTION_OK,

    /* The client connection is finished and we should close the TCP socket. */
    TCP_CONNECTION_CLOSE,

    /* Error opening TCP connection. */
    TCP_CONNECTION_ERR_OPEN,

    /* Error binding TCP socket. */
    TCP_CONNECTION_ERR_BIND,

    /* Error listening on TCP socket. */
    TCP_CONNECTION_ERR_LISTEN,

    /* Error accepting new client connection on TCP socket. */
    TCP_CONNECTION_ERR_ACCEPT,

    /* Error reading PDU from TCP connection */
    TCP_CONNECTION_ERR_READ,

    /* Error writing PDU response to TCP connection */
    TCP_CONNECTION_ERR_WRITE,

    /* PDU status/errors */

    /* Request fully processed */
    TCP_CONNECTION_PDU_STATUS_OK = 100,

    /* not enough infomation was read for a full PDU. */
    TCP_CONNECTION_PDU_INCOMPLETE,

    /* The request had something wrong with it. */
    TCP_CONNECTION_PDU_ERR_MALFORMED,

    /* make any other status start here or above. */
    TCP_CONNECTION_STATUS_LAST = 200,
} tcp_connection_status_t;

typedef enum {
    /* PDU correctly processed. */
    PDU_STATUS_OK = TCP_CONNECTION_PDU_STATUS_OK,

    /* PDU request incomplete/needs more data. */
    PDU_ERR_INCOMPLETE = TCP_CONNECTION_PDU_INCOMPLETE,

    /* Internal error processing PDU */
    PDU_ERR_INTERNAL = TCP_CONNECTION_STATUS_LAST,

    /* PDU has incorrect/illegal parameter value. */
    PDU_ERR_BAD_PARAM,

    /* PDU was malformed in some way. */
    PDU_ERR_MALFORMED,

    /* PDU was recognized by not supported. */
    PDU_ERR_NOT_SUPPORTED,

    /* PDU was not recognized. */
    PDU_ERR_NOT_RECOGNIZED,

    /* Requested entity not found. */
    PDU_ERR_NOT_FOUND,

    /* Response buffer not large enough for response. */
    PDU_ERR_NO_SPACE,

} pdu_status_t;


typedef struct tcp_connection_t *(*tcp_connection_allocate_func)(void *app_data);
typedef tcp_connection_status_t (*tcp_client_handler_func)(slice_p request, slice_p response, struct tcp_connection_t *connection);

typedef struct tcp_connection_t {
    SOCKET sock_fd;

    /* need this in the thread and main loop */
    volatile sig_atomic_t *terminate;

    /* the following must be set up by the allocation function */

    /* data buffer for requests and responses */
    slice_t buffer;

    /* filled in by the application's allocator function */
    tcp_client_handler_func handler;
} tcp_connection_t;

typedef tcp_connection_t *tcp_connection_p;



extern void tcp_server_run(const char *host, const char *port, volatile sig_atomic_t *terminate, tcp_connection_allocate_func allocator, void *app_data);
