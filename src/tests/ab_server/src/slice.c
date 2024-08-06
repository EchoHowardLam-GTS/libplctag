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

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "debug.h"
#include "slice.h"
#include "util.h"


static slice_status_t fix_up_alignment(const char alignment_char, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end);
static slice_status_t decode_signed_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end);
static slice_status_t encode_signed_int(const char int_size_char, bool little_endian, bool word_swap,, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end);
static slice_status_t decode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap,, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end);
static slice_status_t encode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap,, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end);
uint32_t *get_byte_order(size_t size, bool little_endian, bool word_swap);


slice_status_t slice_unpack(slice_t *slice, const char *fmt, ...)
{
    slice_status_t rc = SLICE_STATUS_OK;
    va_list va;
    uint8_t *buf_start = NULL;
    uint8_t *buf_cur = NULL;
    uint8_t *buf_end = NULL;
    size_t fmt_len = 0;
    const char *fmt_end = NULL;
    bool little_endian = true;
    bool word_swap = false;

    if(!slice || !fmt) {
        return SLICE_ERR_NULL_PTR;
    }

    if(slice_len(slice) == 0) {
        return SLICE_ERR_TOO_LITTLE_DATA;
    }

    /* save this for alignment calculations later */
    buf_start = buf_cur = slice->start;
    buf_end = slice->end;

    if((fmt_len = strlen(fmt)) == 0) {
        return SLICE_ERR_TOO_LITTLE_DATA;
    }

    fmt_end = fmt + fmt_len;

    for(const char *fmt_start = fmt; ptr_before(fmt_start, fmt_end) && ptr_before(buf_cur, buf_end) && rc == SLICE_STATUS_OK; fmt_start++) {
        switch(*fmt_start) {
            case ',': break; /* ignore, used for visual separation in format string. */

            case '<':
                little_endian = true;
                break;

            case '>':
                little_endian = false;
                break;

            case '~':
                word_swap = true;
                break;

            case '^': {
                    uint8_t **data_ptr_ptr = va_arg(va, uint8_t **);

                    if(data_ptr_ptr) {
                        *data_ptr_ptr = buf_cur;
                    } else {
                        rc = SLICE_ERR_NULL_PTR;
                    }
                }
                break;

            case 'a': {
                    fmt_start++;

                    if(ptr_before(fmt_start, fmt_end)) {
                        rc = fix_up_alignment(*fmt_start, slice->start, &(buf_cur), buf_end);
                    } else {
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }
                break;

            case 'i': {
                    fmt_start++;

                    if(ptr_before(fmt_start, fmt_end)) {
                        rc = decode_signed_int(*fmt_start, little_endian, word_swap, slice->start, &(buf_cur), buf_end);
                    } else {
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }
                break;

            case 'u': {
                    fmt_start++;

                    if(ptr_before(fmt_start, fmt_end)) {
                        rc = decode_unsigned_int(*fmt_start, slice->start, &(buf_cur), buf_end);
                    } else {
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }
                break;

        }
    }

    va_end(va);

    return rc;
}



slice_status_t slice_pack(slice_t *slice, const char *fmt, ...)
{
    slice_status_t rc = SLICE_STATUS_OK;
    va_list va;
    uint32_t size_of_fmt_string = 0;
    char last_fmt_char = ' ';
    uint64_t last_int_val = 0;

    if(!fmt) {
        return SLICE_ERR_NULL_PTR;
    }

    va_start(va,fmt);

    size_of_fmt_string = strlen(fmt);

    for(uint32_t i=0; i<size_of_fmt_string && rc == SLICE_STATUS_OK; i++) {
        switch(fmt[i]) {
            /* 8-bit unsigned int */
            case 'b': {
                    uint8_t val = va_arg(va, uint8_t);
                    if(buf_cursor(slice) < slice_end(slice)) {
                        slice->data[slice->cursor[slice->tos_index]] = val;
                        slice->cursor[slice->tos_index]++;
                    } else {
                        rc = SLICE_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* 16-bit unsigned int */
            case 'w': {
                    uint16_t val = va_arg(va, uint16_t);
                    if(slice_end(slice) - buf_cursor(slice) >= sizeof(uint16_t)) {
                        slice->data[slice->cursor[slice->tos_index + 0]] = (uint8_t)(val & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 1]] = (uint8_t)((val >> 8) & 0xFF);
                        slice->cursor[slice->tos_index] += sizeof(uint16_t);
                    } else {
                        rc = SLICE_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* 32-bit unsigned int */
            case 'd': {
                    uint32_t val = va_arg(va, uint32_t);
                    if(slice_end(slice) - buf_cursor(slice) >= sizeof(uint32_t)) {
                        slice->data[slice->cursor[slice->tos_index + 0]] = (uint8_t)(val & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 1]] = (uint8_t)((val >> 8) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 2]] = (uint8_t)((val >> 16) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 3]] = (uint8_t)((val >> 24) & 0xFF);
                        slice->cursor[slice->tos_index] += sizeof(uint32_t);
                    } else {
                        rc = SLICE_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* 64-bit unsigned int */
            case 'q': {
                    uint64_t val = va_arg(va, uint64_t);
                    if(slice_end(slice) - buf_cursor(slice) >= sizeof(uint64_t)) {
                        slice->data[slice->cursor[slice->tos_index + 0]] = (uint8_t)(val & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 1]] = (uint8_t)((val >> 8) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 2]] = (uint8_t)((val >> 16) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 3]] = (uint8_t)((val >> 24) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 4]] = (uint8_t)((val >> 32) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 5]] = (uint8_t)((val >> 40) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 6]] = (uint8_t)((val >> 48) & 0xFF);
                        slice->data[slice->cursor[slice->tos_index + 7]] = (uint8_t)((val >> 56) & 0xFF);
                        slice->cursor[slice->tos_index] += sizeof(uint64_t);
                    } else {
                        rc = SLICE_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* c = counted string, p = padded, counted string */
            case 'c':
            case 'p': {
                    /*  */
                    uint16_t str_len = va_arg(va, uint16_t);
                    const char *str_data = va_arg(va, const char *);
                    uint16_t pad = 0;

                    if(fmt[i] == 'p' && (str_len & 0x01)) {
                        pad = 1;
                    }

                    /* enough space? */
                    if(slice_end(slice) - buf_cursor(slice) >= str_len + pad) {
                        /* copy the string data */
                        for(uint16_t i=0; i<str_len; i++) {
                            slice->data[slice->cursor[slice->tos_index]] = (uint8_t)str_data[i];
                            slice->cursor[slice->tos_index]++;
                        }

                        if(pad) {
                            slice->data[slice->cursor[slice->tos_index]] = (uint8_t)0;
                            slice->cursor[slice->tos_index]++;
                        }
                    } else {
                        rc = SLICE_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* nul-terminated C-style string */
            case 'z': {
                    /*  */
                    const char *str_data = va_arg(va, const char *);
                    uint16_t str_len = strlen(str_data);

                    /* enough space? */
                    if(slice_end(slice) - buf_cursor(slice) >= str_len + 1) {
                        /* copy the string data */
                        for(uint16_t i=0; i<str_len; i++) {
                            slice->data[slice->cursor[slice->tos_index]] = (uint8_t)str_data[i];
                            slice->cursor[slice->tos_index]++;
                        }

                        /* zero terminate it. */
                        slice->data[slice->cursor[slice->tos_index]] = (uint8_t)0;
                        slice->cursor[slice->tos_index]++;
                    } else {
                        rc = SLICE_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            default:
                info("WARN: Unsupported format type '%c'!", fmt[i]);
                rc = SLICE_ERR_UNSUPPORTED_FMT;
                break;
        }
    }

    return rc;
}




/****** helpers ******/


slice_status_t fix_up_alignment(const char alignment_char, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end)
{
    slice_status_t rc = SLICE_STATUS_OK;
    uint32_t alignment = 1;

    do {
        switch(alignment_char) {
            case '0':
            case '1': alignment = 1; break;
            case '2': alignment = 2; break;
            case '4': alignment = 4; break;
            case '8': alignment = 8; break;
            default:
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }

        if(rc == SLICE_STATUS_OK && alignment != 1) {
            uint32_t offset = (uint32_t)((intptr_t)(*buf_cur) - (intptr_t)buf_start);
            uint32_t offset_remainder = offset % alignment;

            if(offset_remainder) {
                uint32_t padding = alignment - offset_remainder;

                *buf_cur += padding;

                if(! ptr_before(*buf_cur, buf_end)) {
                    rc = SLICE_ERR_TOO_LITTLE_DATA;
                }
            }
        }
    } while(0);

    return rc;
}


slice_status_t decode_signed_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                int8_t *val = va_arg(va, int8_t *);

                *val = (int8_t)(**buf_cur);

                *buf_cur ++;
            }
            break;

            case '2': {
                    int16_t *val = va_arg(va, int16_t *);
                    uint32_t *byte_order = get_byte_order(sizeof(int16_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(int16_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        *val |= (int16_t)((int16_t)(**buf_cur) << (byte_order[count]));
                    }

                    if(count < sizeof(int16_t)) {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '4': {
                    int32_t *val = va_arg(va, int32_t *);
                    uint32_t *byte_order = get_byte_order(sizeof(int32_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(int32_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        *val |= (int32_t)((int32_t)(**buf_cur) << (byte_order[count]));
                    }

                    if(count < sizeof(int32_t)) {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '8': {
                    int64_t *val = va_arg(va, int64_t *);
                    uint32_t *byte_order = get_byte_order(sizeof(int64_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(int64_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        *val |= (int64_t)((int64_t)(**buf_cur) << (byte_order[count]));
                    }

                    if(count < sizeof(int64_t)) {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            default:
                warn("Unsupported integer length character %c", int_size_char);
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    } while(0);

    return rc;
}


slice_status_t encode_signed_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                int8_t val = va_arg(va, int8_t);

                **buf_cur = (uint8_t)val;

                *buf_cur ++;
            }
            break;

            case '2': {
                    int16_t val = va_arg(va, int16_t);
                    uint32_t *byte_order = get_byte_order(sizeof(int16_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(int16_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        **buf_cur = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(int16_t)) {
                        warn("Insufficient data in buffer to encode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '4': {
                    int32_t val = va_arg(va, int32_t);
                    uint32_t *byte_order = get_byte_order(sizeof(int32_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(int32_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        **buf_cur = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(int32_t)) {
                        warn("Insufficient data in buffer to encode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '8': {
                    int64_t val = va_arg(va, int64_t);
                    uint32_t *byte_order = get_byte_order(sizeof(int64_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(int64_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        **buf_cur = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(int64_t)) {
                        warn("Insufficient data in buffer to encode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;


            default:
                warn("Unsupported integer length character %c", int_size_char);
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    } while(0);

    return rc;
}


slice_status_t decode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                uint8_t *val = va_arg(va, uint8_t *);

                *val = (uint8_t)(**buf_cur);

                *buf_cur ++;
            }
            break;

            case '2': {
                    uint16_t *val = va_arg(va, uint16_t *);
                    uint32_t *byte_order = get_byte_order(sizeof(uint16_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(uint16_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        *val |= (uint16_t)((uint16_t)(**buf_cur) << (byte_order[count]));
                    }

                    if(count < sizeof(uint16_t)) {
                        warn("Insufficient data in buffer to decode unsigned integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '4': {
                    uint32_t *val = va_arg(va, uint32_t *);
                    uint32_t *byte_order = get_byte_order(sizeof(uint32_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(uint32_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        *val |= (uint32_t)((uint32_t)(**buf_cur) << (byte_order[count]));
                    }

                    if(count < sizeof(uint32_t)) {
                        warn("Insufficient data in buffer to decode unsigned integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '8': {
                    uint64_t *val = va_arg(va, uint64_t *);
                    uint32_t *byte_order = get_byte_order(sizeof(uint64_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(uint64_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        *val |= (uint64_t)((uint64_t)(**buf_cur) << (byte_order[count]));
                    }

                    if(count < sizeof(uint64_t)) {
                        warn("Insufficient data in buffer to decode unsigned integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            default:
                warn("Unsupported integer length character %c", int_size_char);
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    } while(0);

    return rc;
}


slice_status_t encode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, uint8_t *buf_start, uint8_t **buf_cur, uint8_t *buf_end)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                uint8_t val = va_arg(va, uint8_t);

                **buf_cur = (uint8_t)val;

                *buf_cur ++;
            }
            break;

            case '2': {
                    uint16_t val = va_arg(va, uint16_t);
                    uint32_t *byte_order = get_byte_order(sizeof(uint16_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(uint16_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        **buf_cur = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(uint16_t)) {
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;

            case '4': {
                    uint32_t val = va_arg(va, uint32_t);
                    uint32_t *byte_order = get_byte_order(sizeof(uint32_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(uint32_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        **buf_cur = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(uint32_t)) {
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;

            case '8': {
                    uint64_t val = va_arg(va, uint64_t);
                    uint32_t *byte_order = get_byte_order(sizeof(uint64_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(uint64_t) && ptr_before(*buf_cur, buf_end); (*buf_cur)++, count++) {
                        **buf_cur = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(uint64_t)) {
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;


            default:
                warn("Unsupported integer length character %c", int_size_char);
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    } while(0);

    return rc;
}






static uint32_t big_endian_byte_order_2_bytes[] = { 1, 0 };
static uint32_t little_endian_byte_order_2_bytes[] = { 0, 1} ;

static uint32_t big_endian_byte_order_4_bytes[] = { 3, 2, 1, 0 };
static uint32_t little_endian_byte_order_4_bytes[] = { 0, 1, 2, 3 };
static uint32_t big_endian_word_swap_byte_order_4_bytes[] = { 1, 0, 3, 2 };
static uint32_t little_endian_word_swap_byte_order_4_bytes[] = { 2, 3, 0, 1 };

static uint32_t big_endian_byte_order_8_bytes[] = { 7, 6, 5, 4, 3, 2, 1, 0 };
static uint32_t little_endian_byte_order_8_bytes[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static uint32_t big_endian_word_swap_byte_order_8_bytes[] = { 5, 4, 7, 6, 1, 0, 3, 2 };
static uint32_t little_endian_word_swap_byte_order_8_bytes[] = { 2, 3, 0, 1, 6, 7, 4, 5 };


static uint32_t *get_byte_order(size_t size, bool little_endian, bool word_swap)
{
    if(size == 2) {
        if(little_endian) {
            return little_endian_byte_order_2_bytes;
        } else {
            return big_endian_byte_order_2_bytes;
        }
    } else if(size == 4) {
        if(little_endian) {
            if(word_swap) {
                return little_endian_word_swap_byte_order_4_bytes;
            } else {
                return little_endian_byte_order_4_bytes;
            }
        } else {
            if(word_swap) {
                return big_endian_word_swap_byte_order_4_bytes;
            } else {
                return big_endian_byte_order_4_bytes;
            }
        }
    } else if(size == 8) {
        if(little_endian) {
            if(word_swap) {
                return little_endian_word_swap_byte_order_8_bytes;
            } else {
                return little_endian_byte_order_8_bytes;
            }
        } else {
            if(word_swap) {
                return big_endian_word_swap_byte_order_8_bytes;
            } else {
                return big_endian_byte_order_8_bytes;
            }
        }
    }

    return NULL;
}
