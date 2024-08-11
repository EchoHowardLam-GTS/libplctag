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

#include "utils/tcp_server.h"
#include "utils/slice.h"

// typedef struct {
//     uint16_t addr_item_type;
//     uint16_t addr_item_size;
//     uint32_t connection_id;
//     uint16_t data_item_type;
//     uint16_t data_item_size;
//     uint16_t connection_seq_num;
// } cpf_header_connected;

// typedef struct {
//     uint16_t addr_item_type;
//     uint16_t addr_item_size;
//     uint16_t data_item_type;
//     uint16_t data_item_size;
// } cpf_header_unconnected;


// typedef union {
//     cpf_header_connected cpf_connected;
//     cpf_header_unconnected cpf_unconnected;
// } cpf_connection_t;

// typedef cpf_connection_t *cpf_connection_p;


typedef struct {
    uint32_t interface_handle;
    uint16_t router_timeout;
    uint16_t item_count;
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint16_t item_data_type;
    uint16_t item_data_length;
} cpf_uc_header_t;

#define CPF_UCONN_HEADER_SIZE (16)

typedef struct {
    uint32_t interface_handle;
    uint16_t router_timeout;
    uint16_t item_count;        /* should be 2 for now. */
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint32_t conn_id;
    uint16_t item_data_type;
    uint16_t item_data_length;
    uint16_t conn_seq;
} cpf_co_header_t;

/* I wish we had variants/sum types! */
typedef struct {
    enum { CPF_CONNECTED, CPF_UNCONNECTED } cpf_type;
    union {
        cpf_uc_header_t uc_header;
        cpf_co_header_t co_header;
    };
} cpf_connection_t;

typedef cpf_connection_t *cpf_connection_p;


#define CPF_CONN_HEADER_SIZE (22)


extern tcp_connection_status_t cpf_dispatch_connected_request(slice_p request, slice_p response, tcp_connection_p connection_arg);
extern tcp_connection_status_t cpf_dispatch_unconnected_request(slice_p request, slice_p response, tcp_connection_p connection_arg);
