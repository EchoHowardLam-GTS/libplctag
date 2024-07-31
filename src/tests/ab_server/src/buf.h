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


enum {
    BUF_OK,
    BUF_ERR_UNSUPPORTED_FMT = 1,
    BUF_ERR_INSUFFICIENT_DATA = 2,
    BUF_ERR_NULL_PTR,
};

#define BUF_STACK_DEPTH (8)

typedef struct {
    uint8_t *data;
    uint8_t tos_index;
    uint16_t capacity[BUF_STACK_DEPTH];
    uint16_t start[BUF_STACK_DEPTH];
    uint16_t end[BUF_STACK_DEPTH];
    uint16_t cursor[BUF_STACK_DEPTH];
} buf_t;

/*
Invariants:

All indexes are absolute

All indexes are uint16_t

Valid values are 0-65534/0xFFFE

Error value is 65535/0xFFFF/UINT16_MAX

start <= end

end <= capacity

start <= cursor <= end

Range of data is [start, end)

*/

inline static buf_t buf_make(uint8_t *data, uint16_t len) {
    buf_t res = {0};

    res.tos_index = 0;

    res.data = data;
    res.capacity[res.tos_index] = len;
    res.start[res.tos_index] = 0;
    res.end[res.tos_index] = 0;
    res.cursor[res.tos_index] = 0;

    return res;
}

inline static bool buf_push(buf_t *buf) {
    bool res = false;

    if(buf) {
        if(buf->tos_index < BUF_STACK_DEPTH - 1) {
            buf->tos_index++;

            buf->capacity[buf->tos_index] = buf->capacity[buf->tos_index - 1];
            buf->start[buf->tos_index] = buf->start[buf->tos_index - 1];
            buf->end[buf->tos_index] = buf->end[buf->tos_index - 1];
            buf->cursor[buf->tos_index] = buf->cursor[buf->tos_index - 1];

            res = true;
        }
    }

    return res;
}

inline static bool buf_pop(buf_t *buf) {
    bool res = false;

    if(buf) {
        if(buf->tos_index > 0) {
            buf->tos_index--;
            res = true;
        }
    }

    return res;
}

inline static uint16_t buf_capacity(buf_t *buf) {
    return (buf ? buf->capacity[buf->tos_index] : UINT16_MAX);
}

inline static bool buf_set_capacity(buf_t *buf, uint16_t capacity) {
    bool res = false;

    if(buf) {
        /* clamp down the end if the capacity is lower */
        if(capacity < buf->end[buf->tos_index]) {
            buf->end[buf->tos_index] = capacity;
        }

        /* clamp down the start if the capacity is lower */
        if(capacity < buf->start[buf->tos_index]) {
            buf->start[buf->tos_index] = capacity;
        }

        /* clamp down the cursor if the capacity is lower */
        if(capacity < buf->cursor[buf->tos_index]) {
            buf->cursor[buf->tos_index] = capacity;
        }

        buf->capacity[buf->tos_index] = capacity;

        res = true;
    }

    return res;
}

inline static uint16_t buf_start(buf_t *buf)  {
    return (buf ? buf->start[buf->tos_index] : UINT16_MAX);
}

inline static uint16_t buf_set_start(buf_t *buf, uint16_t start) {
    bool res = false;

    if(buf) {
        buf->start[buf->tos_index] = start;

        /* clamp down the start if the start is higher than the capacity */
        if(buf->start[buf->tos_index] > buf->capacity[buf->tos_index]) {
            buf->start[buf->tos_index] = buf->capacity[buf->tos_index];
        }

        /* bump up the end if the start is higher */
        if(buf->start[buf->tos_index] > buf->end[buf->tos_index]) {
            buf->end[buf->tos_index] = buf->start[buf->tos_index];
        }

        /* bump up the cursor if the start is higher */
        if(buf->start[buf->tos_index] > buf->cursor[buf->tos_index]) {
            buf->cursor[buf->tos_index] = buf->start[buf->tos_index];
        }

        res = true;
    }

    return res;
}


inline static uint16_t buf_end(buf_t *buf)  {
    return (buf ? buf->end[buf->tos_index] : UINT16_MAX);
}

inline static uint16_t buf_set_end(buf_t *buf, uint16_t end) {
    bool res = false;

    if(buf) {
        buf->end[buf->tos_index] = end;

        /* clamp down the end if the end is higher than the capacity */
        if(buf->end[buf->tos_index] > buf->capacity[buf->tos_index]) {
            buf->end[buf->tos_index] = buf->capacity[buf->tos_index];
        }

        /* clamp down the start if the end is lower */
        if(buf->start[buf->tos_index] > buf->end[buf->tos_index]) {
            buf->start[buf->tos_index] = end;
        }

        /* clamp down the cursor if the end is lower */
        if(buf->cursor[buf->tos_index] > buf->end[buf->tos_index]) {
            buf->cursor[buf->tos_index] = buf->end[buf->tos_index];
        }

        res = true;
    }

    return res;
}


inline static uint16_t buf_cursor(buf_t *buf)  {
    return (buf ? buf->capacity[buf->tos_index] : UINT16_MAX);
}

inline static uint16_t buf_set_cursor(buf_t *buf, uint16_t cursor) {
    bool res = false;

    if(buf) {
        buf->cursor[buf->tos_index] = cursor;

        /* bump up the cursor if the start is higher */
        if(buf->cursor[buf->tos_index] > buf->start[buf->tos_index]) {
            buf->cursor[buf->tos_index] = buf->start[buf->tos_index];
        }

        /* drop the cursor if the end is higher */
        if(buf->cursor[buf->tos_index] > buf->end[buf->tos_index]) {
            buf->cursor[buf->tos_index] = buf->end[buf->tos_index];
        }

        res = true;
    }

    return res;
}


inline static uint16_t buf_peek_byte(buf_t *buf) {
    return (buf && buf->cursor[buf->tos_index] < buf->end[buf->tos_index] ? buf->data[buf->cursor[buf->tos_index]] : UINT16_MAX);
}

extern int16_t buf_unpack(buf_t *buf, const char *fmt, ...);
extern int16_t buf_pack(buf_t *buf, const char *fmt, ...);
