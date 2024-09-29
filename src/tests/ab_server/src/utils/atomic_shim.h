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

#if defined(_WIN32) || defined(_WIN64)

#include <intrin.h>
#include <stdint.h>

typedef volatile int32_t atomic_int32_t;

static inline int32_t atomic_load_int32(atomic_int32_t *ptr)
{
    return _InterlockedOr((volatile long *)ptr, 0);
}

static inline int32_t atomic_compare_and_swap_int32(volatile int32_t *ptr, int32_t expected_val, int32_t new_val)
{
    return _InterlockedCompareExchange((volatile long *)ptr, new_val, expected_val);
}

#else

#include <stdatomic.h>
#include <stdint.h>

typedef volatile int32_t atomic_int32_t;

static inline int32_t atomic_load_int32(atomic_int32_t *ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

int32_t atomic_compare_and_swap_int32(atomic_int32_t *ptr, int32_t expected_val, int32_t new_val)
{
    int32_t actual_val = expected_val;
    __atomic_compare_exchange_n(ptr, &actual_val, new_val, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return actual_val;
}

#endif



static inline int32_t atomic_add_int32(atomic_int32_t *ptr, int32_t value)
{
    int32_t expected_val;
    int32_t new_val;

    do
    {
        expected_val = atomic_load_int32(ptr);
        new_val = expected_val + value;
    } while (atomic_compare_and_swap_int32(ptr, expected_val, new_val) != expected_val);

    return new_val;
}
