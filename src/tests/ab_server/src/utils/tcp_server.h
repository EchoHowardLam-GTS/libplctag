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
    TCP_CLIENT_INCOMPLETE = 1001,
    TCP_CLIENT_PROCESSED = 1002,
    TCP_CLIENT_DONE = 1003,
    TCP_CLIENT_BAD_REQUEST = 1004,
    TCP_CLIENT_UNSUPPORTED = 1005,
} tcp_client_status_t;


typedef struct tcp_client *(*tcp_client_allocate_func)(void *app_data);
typedef tcp_client_status_t (*tcp_client_handler_func)(slice_p request, slice_p response, struct tcp_client *client);

typedef struct tcp_client {
    SOCKET sock_fd;

    /* filled in by the application's allocator function */
    tcp_client_handler_func handler;
    volatile sig_atomic_t *terminate; /* need this in the thread and main loop */

    /* data buffer for requests and responses */
    slice_t buffer;
} tcp_client_t;

typedef tcp_client_t *tcp_client_p;

extern void tcp_server_run(const char *host, const char *port, volatile sig_atomic_t *terminate, tcp_client_allocate_func allocator, void *app_data);
