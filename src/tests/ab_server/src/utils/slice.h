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

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "status.h"


#define SLICE_MAX_LEN (INT32_MAX / 2)
#define SLICE_LEN_ERROR (INT32_MAX)


typedef struct slice_t {
    struct slice_t *parent;
    uint8_t *data;
    uint32_t start;
    uint32_t end;
    status_t status;
} slice_t;

typedef slice_t *slice_p;


#define SLICE_STATIC_INIT_PTR(START, END) { .start = (START), .end = (END) }
#define SLICE_STATIC_INIT_LEN(START, LEN) { .start = (START), .end = ((START + (LEN))) }




extern bool slice_init_parent(slice_p slice, uint8_t *data, uint32_t data_len);
extern bool slice_init_child(slice_p slice, slice_p parent, uint32_t start_offset, uint32_t end_offset);

static inline uint32_t slice_get_start(slice_p slice)
{
    return (slice ? slice->start : SLICE_LEN_ERROR);
}

static inline uint8_t *slice_get_start_ptr(slice_p slice)
{
    return (slice ? (slice->data + slice->start) : NULL);
}

extern status_t slice_set_start(slice_p slice, uint32_t start_abs);
extern status_t slice_set_start_delta(slice_p slice, int32_t start_delta);

static inline uint32_t slice_get_end(slice_p slice)
{
    return (slice ? slice->end : SLICE_LEN_ERROR);
}

static inline uint8_t *slice_get_end_ptr(slice_p slice)
{
    return (slice ? (slice->data + slice->end) : NULL);
}

extern status_t slice_set_end(slice_p slice, uint32_t end_abs);
extern status_t slice_set_end_delta(slice_p slice, int32_t end_delta);

static inline status_t slice_get_status(slice_p slice)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice->status;
}

static inline uint32_t slice_get_len(slice_p slice)
{
    return (slice ? (slice->end - slice->start) : SLICE_LEN_ERROR);
}

/* The new length is start  + new_len and the end offset is changed */
extern bool slice_set_len(slice_p slice, uint32_t new_len);



/* does the slice have data at the offset? */
static inline bool slice_contains_offset(slice_p slice, uint32_t offset)
{
    return slice && (slice->start <= offset && offset < slice->end);
}

static inline bool slice_contains_slice(slice_p outer, slice_p inner)
{
    return outer && inner && (outer->start <= inner->start && inner->end <= outer->end);
}




/*
 *  Data routines and definitions.
 */

typedef enum {
    SLICE_BYTE_ORDER_LE,
    SLICE_BYTE_ORDER_BE,
    SLICE_BYTE_ORDER_WORD_SWAP = 0x1000,
} slice_byte_order_t;


extern uint64_t slice_get_uint(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits);
extern bool slice_set_uint(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits, uint64_t val);

static inline int64_t slice_get_int(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits)
{
    return (int64_t)slice_get_uint(slice, offset, byte_order, num_bits);
}

static inline bool slice_set_int(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits, uint64_t val)
{
    return slice_set_uint(slice, offset, byte_order, num_bits, (uint64_t)val);
}



static inline double slice_get_float(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits)
{
    double d_result = 0.0;

    do {
        uint64_t u_result = 0;

        if(!slice) {
            break;
        }

        if(num_bits != 32 || num_bits != 64) {
            slice->status = STATUS_NOT_SUPPORTED;
            break;
        }

        uint64_t u_result = slice_get_uint(slice, offset, byte_order, num_bits);

        memcpy(&d_result, &u_result, sizeof(d_result));
    } while(0);

    return d_result;
}



static inline bool slice_set_float(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits, double val)
{
    status_t rc = STATUS_OK;

    do {
        uint64_t u_val = 0;

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        if(num_bits != 32 || num_bits != 64) {
            slice->status = STATUS_NOT_SUPPORTED;
            break;
        }

        memcpy(&u_val, &val, sizeof(u_val));

        slice_set_uint(slice, offset, byte_order, num_bits, u_val);

        rc = slice_get_status(slice);
    } while(0);

    return (rc == STATUS_OK);
}


extern bool slice_get_byte_string(slice_p slice, uint8_t *dest, uint32_t dest_size, bool byte_swap);
extern bool slice_set_byte_string(slice_p slice, uint8_t *src, uint32_t src_size, bool byte_swap);