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


#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct buf_t
{
    uint8_t *data;
    int32_t capacity;
    int32_t len;
};

static inline bool buf_init(struct buf_t *buf, uint8_t *data, int32_t capacity)
{
    if (!buf)
    {
        return false;
    }

    if (!*data)
    {
        return false;
    }

    if (capacity < 0)
    {
        return false;
    }

    buf->data = data;
    buf->capacity = capacity;
    buf->len = 0;

    return true;
}

static inline bool buf_shift(struct buf_t *buf, int32_t pivot, int32_t amount)
{
    int32_t new_pivot = 0;

    if (!buf)
    {
        return false;
    }

    if (pivot > buf->len)
    {
        return false;
    }

    if (pivot < 0)
    {
        return false;
    }

    /* do we actually need to do anything? */
    if (amount == 0)
    {
        return true;
    }

    if (pivot + amount < 0)
    {
        return false;
    }

    if (buf->len + amount > buf->capacity)
    {
        return false;
    }

    new_pivot = pivot + amount;

    memmove(buf->data + new_pivot, buf->data + pivot, buf->len - pivot);

    return true;
}

static inline uint16_t le16toh(const uint8_t *data)
{
    return ((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static inline uint32_t le32toh(const uint8_t *data)
{
    return ((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24));
}

static inline uint64_t le64toh(const uint8_t *data)
{
    return (uint64_t)data[0] | ((uint64_t)data[1] << 8) | ((uint64_t)data[2] << 16) | ((uint64_t)data[3] << 24) |
           ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) | ((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
}

static inline void htole16(uint16_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
}

static inline void htole32(uint32_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static inline void htole64(uint64_t value, uint8_t *data)
{
    data[0] = (uint8_t)(value);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
    data[4] = (uint8_t)(value >> 32);
    data[5] = (uint8_t)(value >> 40);
    data[6] = (uint8_t)(value >> 48);
    data[7] = (uint8_t)(value >> 56);
}
