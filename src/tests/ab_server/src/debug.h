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

#include <stdint.h>

typedef enum {
    DEBUG_WARN,
    DEBUG_INFO,
    DEBUG_DETAIL,
    DEBUG_FLOOD,

    DEBUG_ERROR = 1000,
} debug_level_t;

/* debug helpers */
extern void debug_set_level(debug_level_t level);
extern debug_level_t debug_get_level(void);

extern void debug_impl(const char *func, int line, debug_level_t level, const char *templ, ...);

#define error_assert(COND, ...) do { if(!(COND)) { debug_impl(__func__, __LINE__, DEBUG_ERROR, __VA_ARGS__); exit(1); } } while(0)
#define warn(...) debug_impl(__func__, __LINE__, DEBUG_WARN, __VA_ARGS__)
#define info(...) debug_impl(__func__, __LINE__, DEBUG_INFO, __VA_ARGS__)
#define detail(...) debug_impl(__func__, __LINE__, DEBUG_DETAIL, __VA_ARGS__)
#define flood(...) debug_impl(__func__, __LINE__, DEBUG_FLOOD, __VA_ARGS__)

extern void debug_dump_buf(debug_level_t level, uint8_t *start, uint8_t *end);
