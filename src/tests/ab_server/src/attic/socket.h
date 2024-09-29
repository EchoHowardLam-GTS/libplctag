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

#include <stdbool.h>
#include <stdint.h>

#include "slice.h"
#include "status.h"

#ifndef SOCKET
    #define SOCKET int
#endif



extern status_t socket_open(const char *host, const char *port, bool is_server, SOCKET *sock_fd);
extern void socket_close(SOCKET sock);

typedef enum {
    SOCKET_EVENT_NONE = 0,
    SOCKET_EVENT_READ = (1 << 0),
    SOCKET_EVENT_WRITE = (1 << 1),
    SOCKET_EVENT_ACCEPT = (1 << 2),
} socket_event_t;

/* return STATUS_NOT_FOUND if the socket is closed. */
extern status_t socket_event_wait(SOCKET sock, socket_event_t events_wanted, socket_event_t *events_found, uint32_t timeout_ms);

extern status_t socket_accept(SOCKET sock, SOCKET *client_fd);
extern status_t socket_read(SOCKET sock, slice_p in_buf);
extern status_t socket_write(SOCKET sock, slice_p out_buf);
