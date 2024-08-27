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

#include "plc.h"
#include "utils/slice.h"
#include "utils/status.h"
#include "utils/tcp_server.h"



typedef enum {
    EIP_STATUS_SUCCESS = 0x0000,
    EIP_STATUS_UNSUPPORTED = 0x0001,
    EIP_STATUS_NO_RESOURCE = 0x0002,
    EIP_STATUS_BAD_PAYLOAD = 0x0003,
    EIP_STATUS_BAD_PARAM = 0x0064,
    EIP_STATUS_OUT_OF_BOUNDS = 0x0065,
    EIP_STATUS_BAD_VERSION = 0x0069,
    EIP_STATUS_NOT_ALLOWED = 0x006A,
} eip_status_t;



typedef START_PACK struct {
    uint16_le command; /* = EIP_UNCONNECTED_SEND */
    uint16_le length;
    uint32_le session_handle;
    uint32_le status;
    uint64_le sender_context;
    uint32_le options;
} END_PACK eip_header_t;

typedef eip_header_t *eip_header_p;


typedef START_PACK struct {
    uint16_le eip_version;
    uint16_le option_flags;
} END_PACK eip_register_session_t;

typedef eip_register_session_t *eip_register_session_p;


typedef START_PACK struct {
    uint32_le interface_handle;
    uint16_le router_timeout;
    uint16_le item_count;
    uint16_le item_addr_type;
    uint16_le item_addr_length;
    uint32_le conn_id;
    uint16_le item_data_type;
    uint16_le item_data_length;
    uint16_le conn_seq;
} END_PACK cpf_connected_header_t;

typedef cpf_connected_header_t *cpf_connected_header_p;


typedef START_PACK struct {
    uint32_le interface_handle;
    uint16_le router_timeout;
    uint16_le item_count;
    uint16_le item_addr_type;
    uint16_le item_addr_length;
    uint16_le item_data_type;
    uint16_le item_data_length;
} END_PACK cpf_unconnected_header_t;

typedef cpf_unconnected_header_t *cpf_unconnected_header_p;




typedef START_PACK struct {
    uint8_t secs_per_tick;
    uint8_t timeout_ticks;
    uint32_le orig_to_targ_conn_id;
    uint32_le targ_to_orig_conn_id;
    uint16_le conn_serial_number;
    uint16_le orig_vendor_id;
    uint32_le orig_serial_number;
    uint8_t conn_timeout_multiplier;
    uint8_t reserved[3];
    uint32_le orig_to_targ_rpi;
    uint16_le orig_to_targ_conn_params;
    uint32_le targ_to_orig_rpi;
    uint16_le targ_to_orig_conn_params;
    uint8_t transport_class;
} END_PACK cip_forward_open_request_args_t;

typedef cip_forward_open_request_args_t *forward_open_request_args_p;


typedef START_PACK struct {
    uint8_t secs_per_tick;
    uint8_t timeout_ticks;
    uint32_le orig_to_targ_conn_id;
    uint32_le targ_to_orig_conn_id;
    uint16_le conn_serial_number;
    uint16_le orig_vendor_id;
    uint32_le orig_serial_number;
    uint8_t conn_timeout_multiplier;
    uint8_t reserved[3];
    uint32_le orig_to_targ_rpi;
    uint32_le orig_to_targ_conn_params;
    uint32_le targ_to_orig_rpi;
    uint32_le targ_to_orig_conn_params;
    uint8_t transport_class;
} END_PACK cip_forward_open_ex_request_args_t;

typedef cip_forward_open_ex_request_args_t *cip_forward_open_ex_request_args_p;



typedef START_PACK struct {
    uint8_t secs_per_tick;       /* seconds per tick */
    uint8_t timeout_ticks;       /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint16_le conn_serial_number;    /* our connection ID/serial number */
    uint16_le orig_vendor_id;        /* our unique vendor ID */
    uint32_le orig_serial_number;    /* our unique serial number */
} END_PACK cip_forward_close_request_args_t;

typedef cip_forward_close_request_args_t *cip_forward_close_request_args_p;




typedef START_PACK struct {
    uint8_t service;
    uint8_t reserved;
    uint8_t status;
    uint8_t num_extended_status_words;
} END_PACK cip_response_header_t;

typedef cip_response_header_t *cip_response_header_p;




typedef START_PACK struct {
    uint32_le orig_to_targ_conn_id;
    uint32_le targ_to_orig_conn_id;
    uint16_le conn_serial_number;
    uint16_le orig_vendor_id;
    uint32_le orig_serial_number;
    uint32_le orig_to_targ_api;
    uint32_le targ_to_orig_api;
    uint8_t app_data_size;
    uint8_t reserved2;
} END_PACK cip_forward_open_response_t;

typedef cip_forward_open_response_t *cip_forward_open_response_p;



typedef START_PACK struct {
    uint16_le conn_serial_number;
    uint16_le orig_vendor_id;
    uint32_le orig_serial_number;
    uint8_t path_size;
    uint8_t reserved;

} END_PACK cip_forward_close_response_t;

typedef cip_forward_close_response_t *cip_forward_close_response_p;



typedef struct {
    eip_header_t eip_header;

    union {
        eip_register_session_t register_session_request;

        cpf_connected_header_t cpf_connected_header;

        cpf_unconnected_header_t cpf_unconnected_header;
    } eip_command_header;

    union {
        struct {
            uint8_t service;
            slice_t service_epath;

            union {
                cip_forward_open_request_args_t forward_open_request_args;

                cip_forward_open_ex_request_args_t forward_open_ex_request_args;

                cip_forward_close_request_args_t forward_close_request_args;
            } request_args;

            slice_t routing_epath;

        } cip_request;

        struct {
            cip_response_header_t cip_response_header;

            union {
                cip_forward_open_response_t forward_open_response;

                cip_forward_close_response_t forward_close_response;
            } response_args;

            slice_t routing_path;
        } cip_response;
    } cip_pdu;
} eip_pdu_t;

typedef eip_pdu_t *eip_pdu_p;



extern status_t eip_process_pdu(slice_p pdu, app_connection_data_p app_connection_data, app_data_p app_data);
