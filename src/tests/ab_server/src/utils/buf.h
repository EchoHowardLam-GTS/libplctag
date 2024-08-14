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
    uint8_t *cursor;
    uint8_t *end;
} buf_t;

typedef buf_t *buf_p;

#define BUF_STATIC_INIT_PTR(START, END) { .start = (START), .cursor = (START), .end = (END) }
#define BUF_STATIC_INIT_LENGTH(START, SIZE) { .start = (START), .cursor = (START), .end = ((START) + (SIZE)) }

/*
 * Initialize a buffer to the passed start and end pointers.  The cursor is set to the starting
 * pointer.   If there are no errors, return true. If any of the pointers are null, return false.
 * If the start pointer is not less than or equal to the end pointer, return false.
 */
static inline bool buf_init_from_ptr(buf_p buf, uint8_t *start, uint8_t *end)
{
    if(buf && start && end) {
        intptr_t start_int = (intptr_t)start;
        intptr_t end_int = (intptr_t)end;

        if(start_int <= end_int) {
            buf->start = start;
            buf->cursor = start;
            buf->end = end;

            return true;
        }
    }

    return false;
}

static inline bool buf_init_from_length(buf_p buf, uint8_t *start, size_t length)
{
    if(buf && start) {
        return buf_init_from_ptr(buf, start, start + length);
    }

    return false;
}

static inline uint8_t *buf_get_cursor_as_ptr(buf_p buf)
{
    if(buf) {
        return buf->cursor;
    }

    return NULL;
}

#define BUF_GET_CURSOR_FAIL (PTRDIFF_MIN)

/*
 * Return the cursor positions as an offset from the start or
 * BUF_GET_CURSOR_FAIL if the buf pointer is NULL or the offset
 * is negative.
 */
static inline size_t buf_get_cursor_as_offset(buf_p buf)
{
    if(buf) {
        ptrdiff_t diff = buf->cursor - buf->start;

        if(diff > 0) {
            return (size_t)diff;
        }
    }

    return BUF_GET_CURSOR_FAIL;
}

/*
 * Set the cursor.  If it is out of bounds, it will be clamped to either
 * the start or end of the buffer.  True is returned if the cursor can be
 * set.  False if the buffer pointer is NULL.
 */
static inline bool buf_set_cursor_by_ptr(buf_p buf, uint8_t *cursor)
{
    if(buf) {
        intptr_t start_int = (intptr_t)(buf->start);
        intptr_t end_int = (intptr_t)(buf->end);
        intptr_t cursor_int = (intptr_t)(cursor);

        if(cursor_int < start_int) {
            buf->cursor = buf->start;
        } else if(cursor_int > end_int) {
            buf->cursor = buf->end;
        } else {
            buf->cursor = cursor;
        }

        return true;
    }

    return false;
}

static inline bool buf_set_cursor_by_offset(buf_p buf, size_t offset)
{
    if(buf) {
        return buf_set_cursor_by_ptr(buf, buf->start + offset);
    }

    return false;
}

static inline bool buf_clamp_end_to_cursor(buf_p buf)
{
    if(buf) {
        buf->end = buf->cursor;
        return true;
    }

    return false;
}

static inline bool buf_clamp_start_to_cursor(buf_p buf)
{
    if(buf) {
        buf->start = buf->cursor;
        return true;
    }

    return false;
}

#define BUF_LEN_FAIL (PTRDIFF_MAX)

static inline ptrdiff_t buf_len(buf_p buf)
{
    if(buf) {
        return buf->end - buf->start;
    }

    return BUF_LEN_FAIL;
}

static inline ptrdiff_t buf_len_to_cursor(buf_p buf)
{
    if(buf) {
        return buf->cursor - buf->start;
    }

    return INTPTR_MIN;
}

static inline ptrdiff_t buf_len_from_cursor(buf_p buf)
{
    if(buf) {
        return buf->end - buf->cursor;
    }

    return INTPTR_MIN;
}

/*
 * Split the buffer into two parts at the cursor.   If one of the
 * results pointers is NULL, to not set that one.   Returns true
 * on success and false when the source buffer pointer is NULL.
 *
 * The cursors in both parts are set to the part's start pointer.
 */
static inline bool buf_split(buf_p src, buf_p first, buf_p second)
{
    if(src) {
        if(first) {
            first->start = src->start;
            first->end = src->cursor;
            first->cursor = first->start;
        }

        if(second) {
            second->start = src->cursor;
            second->end = src->end;
            second->cursor = second->start;
        }

        return true;
    }

    return false;
}

#define BUF_GET_BYTE_FAIL (UINT16_MAX)

static inline uint16_t buf_get_byte_by_ptr(buf_p buf, uint8_t *ptr)
{
    uint16_t result = BUF_GET_BYTE_FAIL;

    if(buf) {
        intptr_t start_int = (intptr_t)(buf->start);
        intptr_t end_int = (intptr_t)(buf->end);
        intptr_t ptr_int = (intptr_t)(ptr);

        if(ptr_int >= start_int && ptr_int < end_int) {
            result = *ptr;
        }
    }

    return result;
}

static inline uint16_t buf_get_byte_by_offset(buf_p buf, size_t offset)
{
    uint16_t result = BUF_GET_BYTE_FAIL;

    if(buf) {
        return buf_get_byte_by_ptr(buf, buf->start + offset);
    }

    return result;
}

/*
 * Get a byte from the buffer at the cursor.  If the passed boolean flag to change the cursor is
 * set to true, then the function updates the cursor.  If it is false, the cursor will not be changed.
 *
 * If there is no data left (cursor == end) or the buf pointer is NULL then return BUF_GET_BYTE_FAIL.
 */
static inline uint16_t buf_get_byte(buf_p buf, bool change_cursor)
{
    uint16_t result = BUF_GET_BYTE_FAIL;

    if(buf && buf->cursor != buf->end) {
        result = (uint16_t)*(buf->cursor);

        if(change_cursor) {
            ++ buf->cursor;
        }
    }

    return result;
}


static inline bool buf_set_byte_at_ptr(buf_p buf, uint8_t *ptr, uint8_t byte_val)
{
    if(buf && ptr) {
        intptr_t start_int = (intptr_t)(buf->start);
        intptr_t end_int = (intptr_t)(buf->end);
        intptr_t ptr_int = (intptr_t)(ptr);

        if(ptr_int >= start_int && ptr_int < end_int) {
            *ptr = byte_val;

            return true;
        }
    }

    return false;
}

static inline bool buf_set_byte_at_offset(buf_p buf, size_t offset, uint8_t byte_val)
{
    if(buf) {
        return buf_set_byte_at_ptr(buf, buf->start + offset, byte_val);
    }

    return false;
}

/*
 * Set the byte value at the cursor position in the buffer.   If the passed boolean flag is true,
 * the update the cursor position after setting the data.  Return true on success.  If the buffer
 * pointer is NULL, return false.
 */
static inline bool buf_set_byte(buf_p buf, bool change_cursor, uint8_t byte_val)
{
    bool result = false;

    if(buf && buf->cursor != buf->end) {
        *(buf->cursor) = byte_val;

        if(change_cursor) {
            ++ buf->cursor;
        }

        result = true;
    }

    return result;
}


static inline void buf_dump_to_cursor(debug_level_t level, buf_p buf)
{
    if(buf) {
        debug_dump_buf(level, buf->start, buf->cursor);
    } else {
        warn("No data to dump!");
    }
}


static inline void buf_dump_from_cursor(debug_level_t level, buf_p buf)
{
    if(buf) {
        debug_dump_buf(level, buf->cursor, buf->end);
    } else {
        warn("No data to dump!");
    }
}



/* structured data definitions */

typedef enum {
    BUF_VAL_TYPE_NONE = 0,
    BUF_VAL_TYPE_ARRAY,
    BUF_VAL_TYPE_STRUCT,

} buf_val_type_t;

typedef enum {
    BUF_VAL_PROCESS_STATUS_OK,
    BUF_VAL_PROCESS_ERR_INSUFFICIENT_DATA,
    BUF_VAL_PROCESS_ERR_NULL_PTR,
} buf_val_process_status_t;



#define BUF_FIELD_ZERO_SIZE (0)
#define BUF_FIELD_VARIABLE_SIZE (UINT32_MAX)

typedef struct buf_val_def_t {
    const char *name;

    /* flag indicating whether the field is visible or not.   Mostly ignored. */
    bool is_visible;

    /* offset from the start of the value in the encoded data in bytes */
    size_t field_encoded_offset;

    /* size of the field in the encoded data, can be zero or variable! */
    size_t field_encoded_size;

    /* Bit offset within the byte indexed by the encoded data byte offset. */
    uint8_t field_encoded_bit_offset;

    /* offset, in bytes into the decoded value where the data is placed or extracted. */
    size_t field_decoded_offset;

    /* size of the field in the decoded data, can be zero or variable! */
    size_t field_decoded_size;

    /* Bit offset within the byte indexed by the decoded data byte offset. */
    uint8_t field_decoded_bit_offset;

    /**
     * @brief Decode the raw data into a specific host value field.
     *
     * @return - One of:
     *      BUF_VAL_PROCESS_STATUS_OK - processing succeeded.
     *      BUF_VAL_PROCESS_ERR_INSUFFICIENT_DATA - there was not enough data in the buffer to decode the value.
     *      BUF_VAL_PROCESS_ERR_NULL_PTR - one or more pointers were NULL.
     */
    buf_val_process_status_t (*decoder_func)(buf_p buf, struct buf_val_def_t *val_def, size_t val_internal_index, void *val_ptr_arg);

    /**
     * @brief Encode the raw data from a specific host value field.
     *
     * @return - One of:
     *      BUF_VAL_PROCESS_STATUS_OK - processing succeeded.
     *      BUF_VAL_PROCESS_ERR_INSUFFICIENT_DATA - there was not enough data in the buffer to decode the value.
     *      BUF_VAL_PROCESS_ERR_NULL_PTR - one or more pointers were NULL.
     */
    buf_val_process_status_t (*encoder_func)(buf_p buf, struct buf_val_def_t *val_def, size_t val_internal_index, size_t val_element_offset, void *val_ptr_arg);
} buf_val_def_t;

typedef buf_val_def_t *buf_val_def_p;

typedef struct {
    buf_val_type_t val_type;
    union {
        struct {
            uint32_t num_fields;
            buf_val_def_p field_val_defs;
        };
        struct {
            uint32_t num_elements;
            buf_val_def_p element_val_def;
        };
    };
} buf_val_def_t;

typedef buf_val_def_t *buf_val_def_p;

#define BUF_VAL_ALL_ELEMENTS (SIZE_MAX)

extern buf_val_process_status_t buf_get_val_element(buf_p buf, bool change_cursor, buf_val_def_p val_def, size_t field_or_element_index, void *val);
extern buf_val_process_status_t buf_put_val_element(buf_p buf, bool change_cursor, buf_val_def_p val_def, size_t field_or_element_index, void *val);

extern void buf_dump_to_cursor(debug_level_t level, buf_p buf);
extern void buf_dump_from_cursor(debug_level_t level, buf_p buf);
