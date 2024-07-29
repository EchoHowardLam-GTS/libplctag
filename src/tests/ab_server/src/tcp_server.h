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

#pragma once

#include <signal.h>
#include <stdbool.h>
#include "buf.h"
#include "plc.h"

typedef enum {
    TCP_CLIENT_INCOMPLETE = 100001,
    TCP_CLIENT_PROCESSED = 100002,
    TCP_CLIENT_DONE = 100003,
    TCP_CLIENT_BAD_REQUEST = 100004,
    TCP_CLIENT_UNSUPPORTED = 100005
} tcp_client_status_t;

struct tcp_client {
    int sock_fd;
    buf_t buffer;
    buf_t request;
    buf_t response;
    int (*handler)(struct tcp_client *client);
    volatile sig_atomic_t *terminate;
    plc_s *plc;
    void *context;

    /* info for this specific connection */
    struct plc_connection_config conn_config;

    /* if the tag path includes array dimension data then we are not starting at the first element in the tag data. */
    size_t access_offset_bytes;

    uint8_t buffer_data[0];
};

typedef struct tcp_client *tcp_client_p;

extern void tcp_server_run(const char *host, const char *port, int (*handler)(tcp_client_p client), size_t buffer_size, plc_s *plc, volatile sig_atomic_t *terminate, void *context);

// extern void tcp_server_start(tcp_server_p server, volatile sig_atomic_t *terminate);
// extern void tcp_server_destroy(tcp_server_p server);
