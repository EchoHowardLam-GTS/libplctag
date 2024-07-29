/***************************************************************************
 *   Copyright (C) 2020 by Kyle Hayes                                      *
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
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "compat.h"

typedef struct {
    uint8_t *data;
    uint16_t data_capacity;
    uint16_t data_length;
    uint16_t cursor;
} buf_t;

inline static buf_t buf_make(uint8_t *data, uint16_t len) {
    return (buf_t){ .data = data, .data_capacity = len, .data_length = 0, .cursor = 0 };
}

inline static uint16_t buf_len(buf_t *buf) {
    return buf->data_length;
}

inline static bool buf_set_len(buf_t *buf, uint16_t length) {
    bool rc = true;

    if(length <= buf->data_capacity) {
        buf->data_length = length;
        rc = true;
    } else {
        rc = false;
    }

    return rc;
}

inline static uint16_t buf_get_cursor(buf_t *buf) {
    return buf->cursor;
}

inline static bool buf_set_cursor(buf_t *buf, uint16_t cursor) {
    if(cursor < buf->data_length) {
        buf->cursor = cursor;
        return true;
    } else {
        return false;
    }
}

inline static uint16_t buf_cap(buf_t *buf) {
    return buf->data_capacity;
}

inline static uint16_t buf_remaining_space(buf_t *buf) {
    return buf->data_capacity - buf->data_length;
}

inline static bool buf_in_bounds(buf_t *buf, uint16_t range_len) {
    return (buf->data_length - buf->cursor >= range_len) ? true : false;
}


inline static uint16_t buf_get_uint8(buf_t *buf) {
    if(buf_in_bounds(buf, 1)) {
        uint8_t res = buf->data[buf->cursor];
        buf->cursor++;
        return (uint16_t)res;
    } else {
        return UINT16_MAX;
    }
}

inline static bool buf_set_uint8(buf_t *buf, uint8_t val) {
    if(buf_in_bounds(buf, 1)) {
        buf->data[buf->cursor] = val;
        buf->cursor++;
        return true;
    } else {
        return false;
    }
}

inline static uint8_t *buf_peek_bytes(buf_t *buf) {
    return &(buf->data[buf->cursor]);
}

inline static bool buf_match_bytes(buf_t *buf, const uint8_t *data, uint16_t data_len) {
    if(buf->data_length - buf->cursor >= data_len) {
        uint8_t *buf_data = buf_peek_bytes(buf);
        for(size_t i=0; i < data_len; i++) {
            /* fprintf(stderr,"Comparing element %d, %x and %x\n", (int)i, (int)buf_get_uint8(s, (ssize_t)i), data[i]); */
            if(buf_data[i] != data[i]) {
                return false;
            }
        }
        return true;
     } else {
        /* fprintf(stderr, "lengths do not match! Slice has length %d and bytes have length %d!\n", (int)buf_len(s), (int)data_len); */
        return false;
    }
}

inline static bool buf_match_string(buf_t *buf, const char *data) {
    return buf_match_bytes(buf, (const uint8_t*)data, (uint16_t)strlen(data));
}

/* helper functions to get and set data in a slice. */

inline static uint16_t buf_get_uint16_le(buf_t *input_buf) {
    uint16_t res = 0;

    if(buf_in_bounds(input_buf, sizeof(uint16_t))) {
        res = (uint16_t)(buf_get_uint8(input_buf) + (buf_get_uint8(input_buf) << 8));
    }

    return res;
}


inline static uint32_t buf_get_uint32_le(buf_t *input_buf) {
    uint32_t res = 0;

    if(buf_in_bounds(input_buf, sizeof(uint32_t))) {
        res =  (uint32_t)(buf_get_uint8(input_buf))
             + (uint32_t)(buf_get_uint8(input_buf) << 8)
             + (uint32_t)(buf_get_uint8(input_buf) << 16)
             + (uint32_t)(buf_get_uint8(input_buf) << 24);
    }

    return res;
}


inline static uint64_t buf_get_uint64_le(buf_t *input_buf) {
    uint64_t res = 0;

    if(buf_in_bounds(input_buf, sizeof(uint64_t))) {
        res =  ((uint64_t)buf_get_uint8(input_buf))
             + ((uint64_t)buf_get_uint8(input_buf) << 8)
             + ((uint64_t)buf_get_uint8(input_buf) << 16)
             + ((uint64_t)buf_get_uint8(input_buf) << 24)
             + ((uint64_t)buf_get_uint8(input_buf) << 32)
             + ((uint64_t)buf_get_uint8(input_buf) << 40)
             + ((uint64_t)buf_get_uint8(input_buf) << 48)
             + ((uint64_t)buf_get_uint8(input_buf) << 56);
    }

    return res;
}


inline static bool buf_set_uint16_le(buf_t *output_buf, uint16_t val) {
    if(buf_in_bounds(output_buf, sizeof(uint16_t))) {
        buf_set_uint8(output_buf, (uint8_t)(val & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 8) & 0xFF));
        return true;
    } else {
        return false;
    }
}


inline static bool buf_set_uint32_le(buf_t *output_buf, uint32_t val) {
    if(buf_in_bounds(output_buf, sizeof(uint32_t))) {
        buf_set_uint8(output_buf, (uint8_t)(val & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 8) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 16) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 24) & 0xFF));
        return true;
    } else {
        return false;
    }
}


inline static bool buf_set_uint64_le(buf_t *output_buf, uint64_t val) {
    if(buf_in_bounds(output_buf, sizeof(uint64_t))) {
        buf_set_uint8(output_buf, (uint8_t)(val & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 8) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 16) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 24) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 32) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 40) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 48) & 0xFF));
        buf_set_uint8(output_buf, (uint8_t)((val >> 56) & 0xFF));
        return true;
    } else {
        return false;
    }
}
