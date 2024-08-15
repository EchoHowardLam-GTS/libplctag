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
#include <stddef.h>
#include <stdint.h>

#include "debug.h"

typedef struct {
    uint8_t *start;
    uint32_t length;
    uint32_t offset;
} slice_t;

typedef slice_t *slice_p;

#define SLICE_STATIC_INIT_FROM_PTR_LENGTH(START, LENGTH) { .start = (START), .length = (LENGTH), .offset = (0) }
#define SLICE_STATIC_INIT_FROM_PTR_PTR(START, END) { .start = (START), .length = (uint32_t)((END) - (START), .offset = (0)) }


static inline bool slice_init_from_ptr_length(slice_p buf, uint8_t *start, uint32_t length)
{
    if(buf && start) {
        buf->start = start;
        buf->length = (uint32_t)length;
        buf->offset = 0;

        return true;
    }

    return false;
}



/*
 * Initialize a buffer to the passed start and end pointers.  The offset is set to the starting
 * pointer.   If there are no errors, return true. If any of the pointers are null, return false.
 * If the start pointer is not less than or equal to the end pointer, return false.
 */
static inline bool slice_init_from_ptr_ptr(slice_p buf, uint8_t *start, uint8_t *end)
{
    if(buf && start && end) {
        return slice_init_from_ptr_length(buf, start, (uint32_t)(end - start));
    }

    return false;
}


static inline uint8_t *slice_get_offset_as_ptr(slice_p buf)
{
    if(buf) {
        return (buf->start + buf->offset);
    }

    return NULL;
}

#define SLICE_GET_CURSOR_FAIL (PTRDIFF_MIN)

/*
 * Return the offset positions as an offset from the start or
 * SLICE_GET_CURSOR_FAIL if the buf pointer is NULL or the offset
 * is negative.
 */
static inline uint32_t slice_get_offset(slice_p buf)
{
    if(buf) {
        return buf->offset;
    }

    return SLICE_GET_CURSOR_FAIL;
}

/*
 * Set the offset.  If it is out of bounds, it will be clamped to either
 * the start or end of the buffer.  True is returned if the offset can be
 * set.  False if the buffer pointer is NULL.
 */
static inline bool slice_set_offset_by_ptr(slice_p buf, uint8_t *offset_ptr)
{
    if(buf) {
        intptr_t start_int = (intptr_t)(buf->start);
        intptr_t end_int = (intptr_t)(buf->start + buf->length);
        intptr_t offset_int = (intptr_t)(offset_ptr);

        if(offset_int < start_int) {
            buf->offset = 0;
        } else if(offset_int > end_int) {
            buf->offset = buf->length;
        } else {
            buf->offset = (uint32_t)(offset_int - start_int);
        }

        return true;
    }

    return false;
}

static inline bool slice_set_offset(slice_p buf, uint32_t new_offset)
{
    if(buf) {
        if(new_offset <= buf->length) {
            buf->offset = new_offset;
        } else {
            buf->offset = buf->length;
        }
    }

    return false;
}

static inline bool slice_clamp_length_to_offset(slice_p buf)
{
    if(buf) {
        buf->length = buf->offset;

        return true;
    }

    return false;
}

static inline bool slice_clamp_start_to_offset(slice_p buf)
{
    if(buf) {
        buf->start = buf->start + buf->offset;
        return true;
    }

    return false;
}

#define SLICE_LEN_FAIL (PTRDIFF_MAX)

static inline uint32_t slice_len(slice_p buf)
{
    if(buf) {
        return buf->length;
    }

    return SLICE_LEN_FAIL;
}

static inline uint32_t slice_len_to_offset(slice_p buf)
{
    if(buf) {
        return buf->offset;
    }

    return INTPTR_MIN;
}

static inline uint32_t slice_len_from_offset(slice_p buf)
{
    if(buf) {
        return buf->length - buf->offset;
    }

    return INTPTR_MIN;
}

/*
 * Split the buffer into two parts at the offset.   If one of the
 * results pointers is NULL, to not set that one.   Returns true
 * on success and false when the source buffer pointer is NULL.
 *
 * The offsets in both parts are set to the part's start pointer.
 */
static inline bool slice_split(slice_p src, slice_p first, slice_p second)
{
    if(src) {
        if(first) {
            first->start = src->start;
            first->length = src->offset;
            first->offset = 0;
        }

        if(second) {
            second->start = src->start + src->offset;
            second->length = src->length - src->offset;
            second->offset = 0;
        }

        return true;
    }

    return false;
}

#define SLICE_GET_BYTE_FAIL (UINT16_MAX)

// static inline uint16_t slice_get_byte_by_ptr(slice_p buf, uint8_t *ptr)
// {
//     uint16_t result = SLICE_GET_BYTE_FAIL;

//     if(buf) {
//         intptr_t start_int = (intptr_t)(buf->start);
//         intptr_t end_int = (intptr_t)(buf->start + buf->length);
//         intptr_t ptr_int = (intptr_t)(ptr);

//         if(ptr_int >= start_int && ptr_int < end_int) {
//             result = *ptr;
//         }
//     }

//     return result;
// }

static inline uint16_t slice_get_byte_by_offset(slice_p buf, uint32_t offset)
{
    uint16_t result = SLICE_GET_BYTE_FAIL;

    if(buf) {
        if(offset < buf->length) {
            result = *(buf->start + offset);
        }
    }

    return result;
}

/*
 * Get a byte from the buffer at the offset.  If the passed boolean flag to change the offset is
 * set to true, then the function updates the offset.  If it is false, the offset will not be changed.
 *
 * If there is no data left (offset == end) or the buf pointer is NULL then return SLICE_GET_BYTE_FAIL.
 */
static inline uint16_t slice_get_byte(slice_p buf)
{
    uint16_t result = SLICE_GET_BYTE_FAIL;

    if(buf && buf->offset < buf->length) {
        result = (uint16_t)*(buf->start + buf->offset);

        ++ buf->offset;
    }

    return result;
}


// static inline bool slice_set_byte_at_ptr(slice_p buf, uint8_t *ptr, uint8_t byte_val)
// {
//     if(buf && ptr) {
//         intptr_t start_int = (intptr_t)(buf->start);
//         intptr_t end_int = (intptr_t)(buf->start + buf->length);
//         intptr_t ptr_int = (intptr_t)(ptr);

//         if(start_int <= ptr_int && ptr_int < end_int) {
//             *ptr = byte_val;
//             return true;
//         }
//     }

//     return false;
// }

static inline bool slice_set_byte_at_offset(slice_p buf, size_t offset, uint8_t byte_val)
{
    if(buf && offset < buf->length) {
        *(buf->start + offset) = byte_val;
        return true;
    }

    return false;
}

/*
 * Set the byte value at the offset position in the buffer.   If the passed boolean flag is true,
 * the update the offset position after setting the data.  Return true on success.  If the buffer
 * pointer is NULL, return false.
 */
static inline bool slice_set_byte(slice_p buf, uint8_t byte_val)
{
    bool result = false;

    if(buf && buf->offset < buf->length) {
        *(buf->start + buf->offset) = byte_val;

        ++ buf->offset;

        result = true;
    }

    return result;
}


static inline void slice_dump_to_offset(debug_level_t level, slice_p buf)
{
    if(buf) {
        debug_dump_buf(level, buf->start, buf->start + buf->offset);
    } else {
        warn("No data to dump!");
    }
}


static inline void slice_dump_from_offset(debug_level_t level, slice_p buf)
{
    if(buf) {
        debug_dump_buf(level, buf->start + buf->offset, buf->start + buf->length);
    } else {
        warn("No data to dump!");
    }
}



/* data mapping definitions */

typedef enum {
    SLICE_VAL_TYPE_NONE = 0,
    SLICE_VAL_TYPE_SCALAR,
    SLICE_VAL_TYPE_BIT,
    SLICE_VAL_TYPE_ARRAY,
    SLICE_VAL_TYPE_STRUCT,
} slice_val_type_t;

typedef enum {
    SLICE_VAL_PROCESS_STATUS_OK,
    SLICE_VAL_PROCESS_ERR_WRONG_TYPE,
    SLICE_VAL_PROCESS_ERR_INSUFFICIENT_DATA,
    SLICE_VAL_PROCESS_ERR_INSUFFICIENT_SPACE,
    SLICE_VAL_PROCESS_ERR_NULL_PTR,
    SLICE_VAL_PROCESS_ERR_GET,
    SLICE_VAL_PROCESS_ERR_PUT,
    SLICE_VAL_PROCESS_ERR_ALIGN,
} slice_val_process_status_t;



#define SLICE_FIELD_ZERO_SIZE (0)
#define SLICE_FIELD_VARIABLE_SIZE (UINT32_MAX)

typedef struct slice_val_def_t *slice_val_def_p;

typedef struct slice_val_def_t {
    const char *name;

    /* type of the value */
    slice_val_type_t val_type;

    /* common */
    uint8_t encoded_alignment;
    // uint32_t encoded_byte_offset;
    uint32_t encoded_byte_size;

    uint8_t decoded_alignment;
    // uint32_t decoded_byte_offset;
    uint32_t decoded_byte_size;

    /* type-specific */
    union {
        struct {
            uint32_t encoded_bit_offset;
            uint32_t decoded_bit_offset;
        } bit_val;

        struct {
            uint32_t num_elements;
            slice_val_def_p element_type_def;
        } array_val;

        struct {
            uint32_t num_fields;
            slice_val_def_p field_type_defs;
        } struct_val;
    };

    /**
     * @brief Decode the raw data into a value.
     *
     * @return - One of:
     *      SLICE_VAL_PROCESS_STATUS_OK - processing succeeded.
     *      SLICE_VAL_PROCESS_ERR_INSUFFICIENT_DATA - there was not enough data in the buffer to decode the value.
     *      SLICE_VAL_PROCESS_ERR_NULL_PTR - one or more pointers were NULL.
     */
    slice_val_process_status_t (*decoder_func)(struct slice_val_def_t *val_def, slice_p slice_src, slice_p dest);

    /**
     * @brief Encode the value into the data buffer.
     *
     * @return - One of:
     *      SLICE_VAL_PROCESS_STATUS_OK - processing succeeded.
     *      SLICE_VAL_PROCESS_ERR_INSUFFICIENT_DATA - there was not enough data in the buffer to decode the value.
     *      SLICE_VAL_PROCESS_ERR_NULL_PTR - one or more pointers were NULL.
     */
    slice_val_process_status_t (*encoder_func)(struct slice_val_def_t *val_def, slice_p slice_src, slice_p dest);
} slice_val_def_t;

typedef slice_val_def_t *slice_val_def_p;

#define SLICE_VAL_ALL_ELEMENTS (SIZE_MAX)

extern slice_val_process_status_t slice_val_decode(struct slice_val_def_t *val_def, slice_p slice_src, slice_p dest);
extern slice_val_process_status_t slice_val_encode(struct slice_val_def_t *val_def, slice_p slice_src, slice_p dest);

/* basic encoding and decoding functions */
extern slice_val_process_status_t slice_val_decode_u8(struct slice_val_def_t *val_def, slice_p slice_src, slice_p dest);
extern slice_val_process_status_t slice_val_encode_u8(struct slice_val_def_t *val_def, slice_p slice_src, slice_p dest);
