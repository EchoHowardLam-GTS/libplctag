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
#include <string.h>

#include "status.h"


typedef struct slice_t {
    uint8_t *start;
    uint8_t *end;
} slice_t;

typedef slice_t *slice_p;


#define SLICE_STATIC_INIT(START, END) { .start = (START), .end = (END) }


extern status_t slice_init(slice_p slice, uint8_t *start, uint8_t *end)
{
    if(slice) {
        slice->start = start;
        slice->end = end;

        return STATUS_OK;
    }

    return STATUS_NULL_PTR;
}


static inline bool slice_contains_ptr(slice_p slice, uint8_t *ptr)
{
    return slice && ptr && ((intptr_t)(slice->start) <= (intptr_t)(ptr) && (intptr_t)(ptr) < (intptr_t)(slice->end));
}

static inline bool slice_contains_offset(slice_p slice, uint32_t offset)
{
    return slice && ((intptr_t)(slice->start + offset) < (intptr_t)(slice->end));
}

static inline bool slice_contains_slice(slice_p outer, slice_p inner)
{
    return outer && inner && ((intptr_t)(outer->start) <= (intptr_t)(inner->start) && (intptr_t)(inner->end) <= (intptr_t)(outer->end));
}

static inline uint32_t slice_get_len(slice_p slice)
{
    if(slice && slice->start && slice->end) {
        if((intptr_t)(slice->end) >= (intptr_t)(slice->start)) {
            return (uint32_t)((intptr_t)(slice->end) - (intptr_t)(slice->start));
        }
    }

    return 0;
}


static inline uint32_t slice_get_len_to_ptr(slice_p slice, uint8_t *ptr)
{
    if(slice && ptr && slice_contains_ptr(slice, ptr)) {
        return (uint32_t)((intptr_t)(ptr) - (intptr_t)(slice->start));
    }

    return 0;
}

/* this doesn't really do anything but does error check the passed slice pointer */
static inline uint32_t slice_get_len_to_offset(slice_p slice, uint32_t offset)
{
    if(slice) {
        return offset;
    }

    return 0;
}

static inline uint32_t slice_get_len_from_ptr(slice_p slice, uint8_t *ptr)
{
    if(slice && ptr && slice_contains_ptr(slice, ptr)) {
        return (uint32_t)((intptr_t)(slice->end) - (intptr_t)(ptr));
    }

    return 0;
}

static inline uint32_t slice_get_len_from_offset(slice_p slice, uint32_t offset)
{
    if(slice) {
        return slice_get_len_from_ptr(slice, slice->start + offset);
    }

    return 0;
}

extern status_t slice_truncate_to_ptr(slice_p slice, uint8_t *ptr);

static inline status_t slice_truncate_to_offset(slice_p slice, uint32_t offset)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_truncate_to_ptr(slice, slice->start + offset);
}

extern status_t slice_truncate_to_slice_end(slice_p slice, slice_p end_slice);

extern status_t slice_split_at_ptr(slice_p source, uint8_t *cut_ptr, slice_p first_part, slice_p second_part);

extern status_t slice_split_at_offset(slice_p source, uint32_t offset, slice_p first_part, slice_p second_part);

extern status_t slice_split_middle_at_offsets(slice_p slice, uint32_t start_offset, uint32_t end_offset, slice_p first, slice_p second, slice_p third);

extern status_t slice_to_string(slice_p slice, char *result, uint32_t result_size, bool word_swap);
extern status_t string_to_slice(const char *source, slice_p dest, bool word_swap);

extern status_t slice_from_slice(slice_p parent, slice_p new_slice, uint8_t *start, uint8_t *end);


extern status_t slice_get_u8_ptr(slice_p slice, uint8_t *ptr, uint8_t *val);
extern status_t slice_set_u8_ptr(slice_p slice, uint8_t *ptr, uint8_t val);
extern status_t slice_get_u8_offset(slice_p slice, uint32_t offset, uint8_t *val);
extern status_t slice_set_u8_offset(slice_p slice, uint32_t offset, uint8_t val);

extern status_t slice_get_u16_le_at_ptr(slice_p slice, uint8_t *ptr, uint16_t *val);
extern status_t slice_set_u16_le_at_ptr(slice_p slice, uint8_t *ptr, uint16_t val);

static inline status_t slice_get_u16_le_at_offset(slice_p slice, uint32_t offset, uint16_t *val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_get_u16_le_at_ptr(slice, slice->start + offset, val);
}

static inline status_t slice_set_u16_le_at_offset(slice_p slice, uint32_t offset, uint16_t val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_set_u16_le_at_ptr(slice, slice->start + offset, val);
}

extern status_t slice_get_u32_le_at_ptr(slice_p slice, uint8_t *ptr, uint32_t *val);
extern status_t slice_set_u32_le_at_ptr(slice_p slice, uint8_t *ptr, uint32_t val);

static inline status_t slice_get_u32_le_at_offset(slice_p slice, uint32_t offset, uint32_t *val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_get_u32_le_at_ptr(slice, slice->start + offset, val);
}

static inline status_t slice_set_u32_le_at_offset(slice_p slice, uint32_t offset, uint32_t val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_set_u32_le_at_ptr(slice, slice->start + offset, val);
}


extern status_t slice_get_u64_le_at_ptr(slice_p slice, uint8_t *ptr, uint64_t *val);
extern status_t slice_set_u64_le_at_ptr(slice_p slice, uint8_t *ptr, uint64_t val);

static inline status_t slice_get_u64_le_at_offset(slice_p slice, uint32_t offset, uint64_t *val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_get_u64_le_at_ptr(slice, slice->start + offset, val);
}

static inline status_t slice_set_u64_le_at_offset(slice_p slice, uint32_t offset, uint64_t val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_set_u64_le_at_ptr(slice, slice->start + offset, val);
}



static inline status_t slice_get_i16_le_at_ptr(slice_p slice, uint8_t *ptr, int16_t *val)
{
    return slice_get_u16_le_at_ptr(slice, ptr, (uint16_t*)val);
}

static inline status_t slice_set_i16_le_at_ptr(slice_p slice, uint8_t *ptr, int16_t val)
{
    return slice_set_u16_le_at_ptr(slice, ptr, (uint16_t)val);
}

static inline status_t slice_get_i16_le_at_offset(slice_p slice, uint32_t offset, int16_t *val)
{
    return slice_get_u16_le_at_offset(slice, offset, (uint16_t*)val);
}

static inline status_t slice_set_i16_le_at_offset(slice_p slice, uint32_t offset, int16_t val)
{
    return slice_set_u16_le_at_offset(slice, offset, (uint16_t)val);
}

static inline status_t slice_get_i32_le_at_ptr(slice_p slice, uint8_t *ptr, int32_t *val)
{
    return slice_get_u32_le_at_ptr(slice, ptr, (uint32_t*)val);
}

static inline status_t slice_set_i32_le_at_ptr(slice_p slice, uint8_t *ptr, int32_t val)
{
    return slice_set_u32_le_at_ptr(slice, ptr, (uint32_t)val);
}

static inline status_t slice_get_i32_le_at_offset(slice_p slice, uint32_t offset, int32_t *val)
{
    return slice_get_u32_le_at_offset(slice, offset, (uint32_t*)val);
}

static inline status_t slice_set_i32_le_at_offset(slice_p slice, uint32_t offset, int32_t val)
{
    return slice_set_u32_le_at_offset(slice, offset, (uint32_t)val);
}


static inline status_t slice_get_i64_le_at_ptr(slice_p slice, uint8_t *ptr, int64_t *val)
{
    return slice_get_u64_le_at_ptr(slice, ptr, (uint64_t*)val);
}

static inline status_t slice_set_i64_le_at_ptr(slice_p slice, uint8_t *ptr, int64_t val)
{
    return slice_set_u64_le_at_ptr(slice, ptr, (uint64_t)val);
}

static inline status_t slice_get_i64_le_at_offset(slice_p slice, uint32_t offset, int64_t *val)
{
    return slice_get_u64_le_at_offset(slice, offset, (uint64_t*)val);
}

static inline status_t slice_set_i64_le_at_offset(slice_p slice, uint32_t offset, int64_t val)
{
    return slice_set_u64_le_at_offset(slice, offset, (uint64_t)val);
}



extern status_t slice_get_f32_le_at_ptr(slice_p slice, uint8_t *ptr, float *val);
extern status_t slice_set_f32_le_at_ptr(slice_p slice, uint8_t *ptr, float val);

static inline status_t slice_get_f32_le_at_offset(slice_p slice, uint32_t offset, float *val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_get_f32_le_at_ptr(slice, slice->start + offset, val);
}

static inline status_t slice_set_f32_le_at_offset(slice_p slice, uint32_t offset, float val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_set_f32_le_at_ptr(slice, slice->start + offset, val);
}


extern status_t slice_get_f64_le_at_ptr(slice_p slice, uint8_t *ptr, double *val);
extern status_t slice_set_f64_le_at_ptr(slice_p slice, uint8_t *ptr, double val);

static inline status_t slice_get_f64_le_at_offset(slice_p slice, uint32_t offset, double *val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_get_f64_le_at_ptr(slice, slice->start + offset, val);
}

static inline status_t slice_set_f64_le_at_offset(slice_p slice, uint32_t offset, float val)
{
    if(!slice) {
        return STATUS_NULL_PTR;
    }

    return slice_set_f64_le_at_ptr(slice, slice->start + offset, val);
}
