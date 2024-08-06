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



typedef enum {
    SLICE_STATUS_OK,
    SLICE_ERR_NULL_PTR,
    SLICE_ERR_TOO_LITTLE_DATA,
    SLICE_ERR_TOO_MUCH_DATA,
    SLICE_ERR_UNSUPPORTED_FORMAT,
    SLICE_ERR_INCOMPLETE_FORMAT,
    SLICE_ERR_OUT_OF_BOUNDS,
} slice_status_t;


typedef struct {
    uint8_t *start;
    uint8_t *end;
} slice_t;

typedef slice_t *slice_p;

#define SLICE_STATIC_INIT(START, END) { .start = (START), .end = (END) }

static inline slice_status_t slice_init(slice_p slice, uint8_t *start, uint8_t *end)
{
    if(slice) {
        slice->start = start;
        slice->end = end;

        return SLICE_STATUS_OK;
    }

    return SLICE_ERR_NULL_PTR;
}

static inline slice_status_t slice_contains(slice_p slice, uint8_t *ptr)
{
    if(slice && ptr) {
        intptr_t start_int = (intptr_t)(slice->start);
        intptr_t end_int = (intptr_t)(slice->end);
        intptr_t ptr_int = (intptr_t)(ptr);

        if(start_int <= ptr_int && ptr_int < end_int) {
            return SLICE_STATUS_OK;
        } else {
            return SLICE_ERR_OUT_OF_BOUNDS;
        }
    }

    return SLICE_ERR_NULL_PTR;
}


static inline uint32_t slice_len(slice_p slice)
{
    if(slice && slice->start && slice->end) {
        return (uint32_t)(slice->end - slice->start);
    }

    return SLICE_ERR_NULL_PTR;
}


/*
 * Format characters
 *
 * pack and unpack
 *
 * , = ignored, used as visual separation in format string.
 *
 * < - set encode/decode to little-endian. No result.
 *
 * > - set encode/decode to big-endian.  No result.
 *
 * ~ - swap words, only for the next entry
 *
 * ^ - save the pointer to the current position in the slice.
 *      pack: a pointer to a pointer to uint8_t.
 *    unpack: a pointer to a pointer to uint8_t.
 *
 * a[1,2,4,8] - align to specified byte boundary. Pad with zero bytes.  No result saved.
 *
 * i[1,2,4,8] - signed integer of specificed size in bytes.
 *      pack: takes a signed integer of the specified size, int8_t, int16_t...
 *    unpack: takes a pointer to a signed integer of the specified size
 *
 * u[1,2,4,8] - as for above, but unsigned.
 *
 * f[4,8] - 4 or 8-byte floating point.
 *      pack: takes a 4 or 8-byte float/double.
 *    unpack: takes a pointer to a float or double.
 *
 * c[1,2,4,8] - counted byte string.  Consumes/produces the count word and the following
 *          count bytes.
 *          pack: takes a pointer to a slice with valid (non-zero) start and end pointers.
 *                If the slice has more data than the count word can encode, pack returns
 *                SLICE_ERR_TOO_MUCH_DATA.
 *        unpack: takes a pointer to a slice. If the slice has non-null start and end, the
 *                slice is used to store the byte string.  The end pointer is adjusted to
 *                point to the end of the byte string.   If the slice has null start and
 *                end, memory for the byte string will be allocated and the slice will be
 *                updated to point to that memory.
 *
 *                If the slice does not contain enough space for the byte string data, the
 *                error SLICE_ERR_TOO_MUCH_DATA is returned.
 *
 * th - Value-terminated byte string.   The "h" is a pair of hex digits that denote the byte
 *      value on which to terminate.
 *        pack: takes a slice pointer. The slice contains the byte string. The data must contain
 *              the terminator byte as well.
 *      unpack: takes a slice pointer.  If start and end are non-null, the data is copied into
 *              the slice. The end pointer is adjusted to point to the end of the copied data.
 *              If there is insufficient space in the slice, unpack returns SLICE_ERR_TOO_MUCH_DATA.
 *              If start and end are null, memory is allocated for the byte string and the
 *              slice is updated to point to that memory. The terminator byte is included in the
 *              copied data.
 *
 * e[0,1] - EPATH byte string. The EPATH starts with a single byte count of the number of 16-bit
 *          words in the EPATH.  The EPATH can be padded (1) or unpadded (0). A padded EPATH will
 *          have a zero byte after the count word.
 *          pack: takes a pointer to a slice.  If the data in the slice is longer than 510 bytes
 *                pack returns SLICE_ERR_TOO_MUCH_DATA as the length cannot be represented in one
 *                byte. The count word and data in the slice are copied into the slicefer.
 *        unpack: takes a pointer to a slice. If the slice has non-null start and end the EPATH
 *                data, EXCLUDING THE COUNT BYTE, is copied into the slice.   If there is not
 *                enough space, the error SLICE_ERR_TOO_MUCH_DATA is returned.  If the slice start
 *                and end are null, memory is allocated for the EPATH data and the slice is set
 *                to point to it.  The data is copied into the newly allocated memory. The count
 *                byte is not copied.
 *
 */

extern slice_status_t slice_unpack(slice_t *slice, const char *fmt, ...);
extern slice_status_t slice_pack(slice_t *slice, const char *fmt, ...);
