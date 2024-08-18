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

#include <stddef.h>
#include <stdint.h>

#include "utils/mutex_compat.h"
// #include "eip.h"
// #include "cpf.h"
// #include "cip.h"
// #include "pccc.h"


typedef uint16_t cip_tag_type_t;

/* CIP data types. */
const cip_tag_type_t TAG_CIP_TYPE_BOOL        = ((cip_tag_type_t)0x00C1); /* 8-bit boolean value */
const cip_tag_type_t TAG_CIP_TYPE_SINT        = ((cip_tag_type_t)0x00C2); /* Signed 8–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_INT         = ((cip_tag_type_t)0x00C3); /* Signed 16–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_DINT        = ((cip_tag_type_t)0x00C4); /* Signed 32–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_LINT        = ((cip_tag_type_t)0x00C5); /* Signed 64–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_USINT       = ((cip_tag_type_t)0x00C6); /* Unsigned 8–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_UINT        = ((cip_tag_type_t)0x00C7); /* Unsigned 16–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_UDINT       = ((cip_tag_type_t)0x00C8); /* Unsigned 32–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_ULINT       = ((cip_tag_type_t)0x00C9); /* Unsigned 64–bit integer value */
const cip_tag_type_t TAG_CIP_TYPE_REAL        = ((cip_tag_type_t)0x00CA); /* 32–bit floating point value, IEEE format */
const cip_tag_type_t TAG_CIP_TYPE_LREAL       = ((cip_tag_type_t)0x00CB); /* 64–bit floating point value, IEEE format */

// FIXME - this is wrong below!
const cip_tag_type_t TAG_CIP_TYPE_STRING      = ((cip_tag_type_t)0x00D0); /* 88-byte string, with 82 bytes of data, 4-byte count and 2 bytes of padding */

/* this is needed for Rockwell-style BOOL arrays. */
const cip_tag_type_t TAG_CIP_TYPE_32BIT_BIT_STRING = ((cip_tag_type_t)0x00D3); /* used for Rockwell bit strings */

/* PCCC data types.   FIXME */
typedef uint8_t pccc_tag_type_t;

const pccc_tag_type_t TAG_PCCC_TYPE_INT         = ((pccc_tag_type_t)0x89); /* Signed 16–bit integer value */
const pccc_tag_type_t TAG_PCCC_TYPE_DINT        = ((pccc_tag_type_t)0x91); /* Signed 32–bit integer value */
const pccc_tag_type_t TAG_PCCC_TYPE_REAL        = ((pccc_tag_type_t)0x8a); /* 32–bit floating point value, IEEE format */
const pccc_tag_type_t TAG_PCCC_TYPE_STRING      = ((pccc_tag_type_t)0x8d); /* 82-byte string with 2-byte count word. */


struct tag_def_t {
    struct tag_def_t *next_tag;
    char *name;
    mutex_t mutex; /* only one thread at a time can access. */
    cip_tag_type_t tag_type;
    size_t elem_size;
    size_t elem_count;
    size_t data_file_num;
    size_t num_dimensions;
    size_t dimensions[3];
    uint8_t *data;
};

typedef struct tag_def_t tag_def_t;
typedef struct tag_def_t *tag_def_p;

typedef enum plc_type_t {
    PLC_TYPE_NONE,
    PLC_TYPE_CONTROL_LOGIX,
    PLC_TYPE_MICRO800,
    PLC_TYPE_OMRON,
    PLC_TYPE_PLC5,
    PLC_TYPE_SLC,
    PLC_TYPE_MICROLOGIX
} plc_type_t;


const uint32_t MAX_DEVICE_BUFFER_SIZE = (8192);

const uint8_t MAX_CIP_DEVICE_PATH_WORDS = (20);

typedef struct {
    struct tcp_connection_t tcp_connection;

    /* PLC info */
    plc_type_t plc_type;
    const char *port_string;
    tag_def_p tags;

    /* EIP info */
    uint32_t session_handle;
    uint64_t sender_context;

    /* CIP info */
    uint32_t client_to_server_max_packet;
    uint32_t server_to_client_max_packet;

    /* CIP device path */
    bool needs_path;
    uint8_t path[MAX_CIP_DEVICE_PATH_WORDS*2];
    uint8_t path_len;

    /* CIP connection info */
    bool has_cip_connection;

    uint32_t client_connection_id;
    uint16_t client_connection_seq;
    uint32_t server_connection_id;
    uint16_t server_connection_seq;

    /* debugging */
    uint32_t response_delay;
    int reject_fo_count;

    /* buffer for requests and responses */
    uint8_t pdu_data_buffer[MAX_DEVICE_BUFFER_SIZE];
} plc_connection_t;

typedef plc_connection_t *plc_connection_p;


#define GET_FIELD(SLICE, TYPE, ADDR, SIZE)                                    \
        if(slice_get_ ## TYPE ## _le_at_offset((SLICE), offset, (ADDR))) {    \
            warn("Unable to get field at offset %"PRIu32"!", offset);         \
            rc = STATUS_ERR_OP_FAILED;                                            \
            break;                                                            \
        }                                                                     \
        offset += (SIZE)


#define SET_FIELD(SLICE, TYPE, VAL, SIZE)                                     \
        if(slice_set_ ## TYPE ## _le_at_offset((SLICE), offset, (VAL))) {     \
            warn("Unable to set field at offset %"PRIu32"!", offset);         \
            rc = STATUS_ERR_OP_FAILED;                                            \
            break;                                                            \
        }                                                                     \
        offset += (SIZE)





#define assert_warn(COND, STATUS, ... ) if(!(COND)) {            \
    debug_impl(__func__, __LINE__, DEBUG_WARN, __VA_ARGS__);    \
    rc = (STATUS);                                              \
    break;                                                      \
} do {} while(0)

#define assert_info(COND, STATUS, ... ) if(!(COND)) {            \
    debug_impl(__func__, __LINE__, DEBUG_INFO, __VA_ARGS__);    \
    rc = (STATUS);                                              \
    break;                                                      \
} do {} while(0)

#define assert_detail(COND, STATUS, ... ) if(!(COND)) {          \
    debug_impl(__func__, __LINE__, DEBUG_DETAIL, __VA_ARGS__);  \
    rc = (STATUS);                                              \
    break;                                                      \
} do {} while(0)

#define assert_flood(COND, STATUS, ... ) if(!(COND)) {          \
    debug_impl(__func__, __LINE__, DEBUG_FLOOD, __VA_ARGS__);   \
    rc = (STATUS);                                              \
    break;                                                      \
} do {} while(0)
