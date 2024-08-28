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
#include "utils/status.h"
#include "utils/tcp_server.h"

#include "plc.h"


typedef enum {
    CIP_SERVICE_MULTI_REQUEST = 0x0A,
    CIP_SERVICE_PCCC_EXECUTE = 0x4B,
    CIP_SERVICE_READ_TAG = 0x4C,
    CIP_SERVICE_WRITE_TAG = 0x4D,
    CIP_SERVICE_FORWARD_CLOSE = 0x4E, /* DUPE !*/
    CIP_SERVICE_RMW_TAG = 0x4E,  /* DUPE ! */
    CIP_SERVICE_READ_TAG_FRAG = 0x52,
    CIP_SERVICE_WRITE_TAG_FRAG = 0x53,
    CIP_SERVICE_FORWARD_OPEN = 0x54,
    CIP_SERVICE_FORWARD_OPEN_EX = 0x5B,
    CIP_SERVICE_LIST_TAG_ATTRIBS = 0x55,
} cip_service_type_t;


enum {
    CIP_OK = 0,
    CIP_ERR_COMMS = 0x01,
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


typedef START_PACK struct {
    uint8_t service;
    uint8_t epath_word_count;
    uint8_t epath_bytes[];
} END_PACK cip_request_header_t;



typedef START_PACK struct {
    uint8_t service;
    uint8_t reserved;
    uint8_t status;
    uint8_t ext_status_word_count;
    uint16_le ext_status_words[];
} END_PACK cip_response_header_t;



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
    uint8_t target_epath_word_count;
    uint8_t reserved_target_epath_padding;
    uint8_t target_epath_bytes[];
} END_PACK cip_forward_open_request_args_t;

typedef cip_forward_open_request_args_t *cip_forward_open_request_args_p;


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
    uint8_t target_epath_word_count;
    uint8_t reserved_target_epath_padding;
    uint8_t target_epath_bytes[];
} END_PACK cip_forward_open_ex_request_args_t;

typedef cip_forward_open_ex_request_args_t *cip_forward_open_ex_request_args_p;





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
    uint8_t secs_per_tick;       /* seconds per tick */
    uint8_t timeout_ticks;       /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint16_le conn_serial_number;    /* our connection ID/serial number */
    uint16_le orig_vendor_id;        /* our unique vendor ID */
    uint32_le orig_serial_number;    /* our unique serial number */
} END_PACK cip_forward_close_request_args_t;

typedef cip_forward_close_request_args_t *cip_forward_close_request_args_p;




typedef START_PACK struct {
    uint16_le conn_serial_number;
    uint16_le orig_vendor_id;
    uint32_le orig_serial_number;
    uint8_t path_size;
    uint8_t reserved;
} END_PACK cip_forward_close_response_t;

typedef cip_forward_close_response_t *cip_forward_close_response_p;




extern status_t cip_process_request(slice_p encoded_pdu_data, plc_connection_p connection);
