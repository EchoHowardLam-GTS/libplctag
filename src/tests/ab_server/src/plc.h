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
// #include "utils/tcp_server.h"

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
    // struct tcp_connection_t tcp_connection;

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
    // bool has_cip_connection;

    uint8_t secs_per_tick;
    uint8_t timeout_ticks;
    uint32_t orig_to_targ_conn_id;
    uint32_t targ_to_orig_conn_id;
    uint16_t conn_serial_number;
    uint16_t orig_vendor_id;
    uint32_t orig_serial_number;
    uint8_t conn_timeout_multiplier;
    uint32_t orig_to_targ_rpi;
    uint16_t orig_to_targ_conn_params;
    uint32_t targ_to_orig_rpi;
    uint16_t targ_to_orig_conn_params;
    uint8_t transport_class;

    uint16_t orig_to_targ_conn_seq;
    uint16_t targ_to_orig_conn_seq;

    /* debugging */
    uint32_t response_delay;
    int reject_fo_count;

    /* buffer for requests and responses */
    uint8_t pdu_data_buffer[MAX_DEVICE_BUFFER_SIZE];
} plc_t;

typedef plc_t *plc_p;


// struct pdu_t {
//     slice_t request_header_slice;
//     slice_t request_payload_slice;
//     slice_t response_header_slice;
//     slice_t response_payload_slice;
// };

// typedef struct pdu_t pdu_t;
// typedef pdu_t *pdu_p;


// extern status_t clean_up_plc_connection_data(app_connection_data_p app_connection_data, app_data_p app_data);
// extern status_t init_plc_connection_data(app_connection_data_p app_connection_data, app_data_p app_data);




START_PACK typedef struct {
    union {
        uint8_t b_val[2];
        uint16_t u_val;
    } val;
} END_PACK uint16_le;


START_PACK typedef struct {
    union {
        uint8_t b_val[4];
        uint32_t u_val;
    } val;
} END_PACK uint32_le;



START_PACK typedef struct {
    union {
        uint8_t b_val[8];
        uint64_t u_val;
    } val;
} END_PACK uint64_le;




inline static uint16_le h2le16(uint16_t val)
{
    uint16_le result;

    result.val.b_val[0] = (uint8_t)(val & 0xFF);
    result.val.b_val[1] = (uint8_t)((val >> 8) & 0xFF);

    return result;
}


inline static uint16_t le2h16(uint16_le src)
{
    uint16_t result = 0;

    result = (uint16_t)(src.val.b_val[0] + ((src.val.b_val[1]) << 8));

    return result;
}




inline static uint32_le h2le32(uint32_t val)
{
    uint32_le result;

    result.val.b_val[0] = (uint8_t)(val & 0xFF);
    result.val.b_val[1] = (uint8_t)((val >> 8) & 0xFF);
    result.val.b_val[2] = (uint8_t)((val >> 16) & 0xFF);
    result.val.b_val[3] = (uint8_t)((val >> 24) & 0xFF);

    return result;
}


inline static uint32_t le2h32(uint32_le src)
{
    uint32_t result = 0;

    result |= (uint32_t)(src.val.b_val[0]);
    result |= ((uint32_t)(src.val.b_val[1]) << 8);
    result |= ((uint32_t)(src.val.b_val[2]) << 16);
    result |= ((uint32_t)(src.val.b_val[3]) << 24);

    return result;
}






inline static uint64_le h2le64(uint64_t val)
{
    uint64_le result;

    result.val.b_val[0] = (uint8_t)(val & 0xFF);
    result.val.b_val[1] = (uint8_t)((val >> 8) & 0xFF);
    result.val.b_val[2] = (uint8_t)((val >> 16) & 0xFF);
    result.val.b_val[3] = (uint8_t)((val >> 24) & 0xFF);
    result.val.b_val[4] = (uint8_t)((val >> 32) & 0xFF);
    result.val.b_val[5] = (uint8_t)((val >> 40) & 0xFF);
    result.val.b_val[6] = (uint8_t)((val >> 48) & 0xFF);
    result.val.b_val[7] = (uint8_t)((val >> 56) & 0xFF);

    return result;
}


inline static uint64_t le2h64(uint64_le src)
{
    uint64_t result = 0;

    result |= (uint64_t)(src.val.b_val[0]);
    result |= ((uint64_t)(src.val.b_val[1]) << 8);
    result |= ((uint64_t)(src.val.b_val[2]) << 16);
    result |= ((uint64_t)(src.val.b_val[3]) << 24);
    result |= ((uint64_t)(src.val.b_val[4]) << 32);
    result |= ((uint64_t)(src.val.b_val[5]) << 40);
    result |= ((uint64_t)(src.val.b_val[6]) << 48);
    result |= ((uint64_t)(src.val.b_val[7]) << 56);

    return result;
}





#define GET_UINT_FIELD(SLICE, VAR)                                                                  \
        (VAR) = (TYPEOF(VAR))slice_get_uint(SLICE, offset, SLICE_BYTE_ORDER_LE, (sizeof(VAR) * 8)); \
        offset += (uint32_t)(sizeof(VAR));                                                          \
        if((rc = slice_get_status(SLICE)) != STATUS_OK) {                                           \
            warn("Error %s getting " #VAR ".", status_to_str);                                      \
            break;                                                                                  \
        } do{}while(0)

#define SET_UINT_FIELD(SLICE, VAR)                                                              \
        slice_set_uint(SLICE, offset, SLICE_BYTE_ORDER_LE, (sizeof(VAR) * 8), (uint64_t)(VAR)); \
        offset += (uint32_t)(sizeof(VAR));                                                      \
        if((rc = slice_get_status(SLICE)) != STATUS_OK) {                                       \
            warn("Error %s setting " #VAR ".", status_to_str);                                  \
            break;                                                                              \
        } do{}while(0)


// #define GET_FIELD(SLICE, TYPE, ADDR, SIZE)                                                          \
//         if((rc = slice_get_ ## TYPE ## _le_at_offset((SLICE), offset, (ADDR))) != STATUS_OK) {      \
//             warn("Unable to get field at offset %"PRIu32"! Error: %s!", offset, status_to_str(rc)); \
//            break;                                                                                   \
//         }                                                                                           \
//         offset += (SIZE)


// #define SET_FIELD(SLICE, TYPE, VAL, SIZE)                                                           \
//         if((rc = slice_set_ ## TYPE ## _le_at_offset((SLICE), offset, (VAL))) != STATUS_OK) {       \
//             warn("Unable to set field at offset %"PRIu32"! Error: %s!", offset, status_to_str(rc)); \
//             break;                                                                                  \
//         }                                                                                           \
//         offset += (SIZE)





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
