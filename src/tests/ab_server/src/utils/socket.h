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

#include <stdint.h>
#include "slice.h"

#ifndef SOCKET
    #define SOCKET int
#endif

typedef enum {
    SOCKET_STATUS_OK,
    SOCKET_ERR_STARTUP,
    SOCKET_ERR_OPEN,
    SOCKET_ERR_CREATE,
    SOCKET_ERR_BIND,
    SOCKET_ERR_LISTEN,
    SOCKET_ERR_SETOPT,
    SOCKET_ERR_READ,
    SOCKET_ERR_WRITE,
    SOCKET_ERR_SELECT,
    SOCKET_ERR_ACCEPT,
    SOCKET_ERR_TIMEOUT,
    SOCKET_ERR_BAD_FD,
    SOCKET_ERR_BAD_PARAM,
    SOCKET_ERR_NO_MEMORY,
} socket_status_t;

extern socket_status_t socket_open(const char *host, const char *port, SOCKET *sock_fd);
extern void socket_close(SOCKET sock);
extern socket_status_t socket_accept(SOCKET sock, SOCKET *client_fd, uint32_t timeout_ms);
extern socket_status_t socket_read(SOCKET sock, slice_p in_buf, uint32_t timeout_ms);
extern socket_status_t socket_write(SOCKET sock, slice_p out_buf, uint32_t timeout_ms);
