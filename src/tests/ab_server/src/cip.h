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

#include "utils/slice.h"
#include "utils/tcp_server.h"

enum {
    CIP_OK = 0,
    CIP_ERR_FLAG = 0x01,
    CIP_ERR_PATH_DEST_UNKNOWN = 0x05,
    CIP_ERR_FRAG = 0x06,
    CIP_ERR_UNSUPPORTED = 0x08,
    CIP_ERR_INSUFFICIENT_DATA = 0x13,
    CIP_ERR_INVALID_PARAMETER = 0x20,

    CIP_ERR_EXTENDED = 0xFF,

    /* extended errors */
    CIP_ERR_EX_TOO_LONG = 0x2105,
    CIP_ERR_EX_DUPLICATE_CONN = 0x100,
};

#define MAX_CIP_DEVICE_PATH (20)

typedef struct {
    uint8_t path[MAX_CIP_DEVICE_PATH];
    uint8_t path_len;

    uint32_t client_connection_id;
    uint16_t client_connection_seq;
    uint16_t client_connection_serial_number;
    uint16_t client_vendor_id;
    uint32_t client_serial_number;
    uint32_t client_to_server_rpi;
    uint32_t client_to_server_max_packet;

    uint32_t server_connection_id;
    uint16_t server_connection_seq;
    uint32_t server_to_client_rpi;
    uint32_t server_to_client_max_packet;

    bool is_connected;

    /* debug */
    int reject_fo_count;
} cip_connection_t;

typedef cip_connection_t *cip_connection_p;

extern tcp_connection_status_t cip_dispatch_request(slice_p request, slice_p response, tcp_connection_p connection_arg);
