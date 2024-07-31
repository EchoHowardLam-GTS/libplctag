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
#include "buf.h"


int32_t buf_unpack(buf_t *buf, const char *fmt, ...)
{
    int32_t rc = BUF_OK;
    va_list va;
    uint32_t size_of_fmt_string = 0;
    char last_fmt_char = ' ';
    uint64_t last_int_val = 0;

    if(!fmt) {
        return BUF_ERR_NULL_PTR;
    }

    va_start(va,fmt);

    size_of_fmt_string = strlen(fmt);

    for(uint32_t i=0; i<size_of_fmt_string && rc == BUF_OK; i++) {
        switch(fmt[i]) {
            /* 8-bit unsigned int */
            case 'b': {
                    uint8_t *dest = va_arg(va, uint8_t*);
                    if(buf_cursor(buf) < buf_end(buf)) {
                        *dest = buf->data[buf->cursor[buf->tos_index]];
                        last_int_val = (uint64_t)(*dest);
                        buf->cursor[buf->tos_index]++;
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }
                break;

            /* 16-bit unsigned int */
            case 'w': {
                    uint16_t *dest = va_arg(va, uint16_t*);
                    if(buf_end(buf) - buf_cursor(buf) >= sizeof(uint16_t)) {
                        *dest = (buf->data[buf->cursor[buf->tos_index + 0]]) +
                                (buf->data[buf->cursor[buf->tos_index + 1]] << 8);
                        last_int_val = (uint64_t)(*dest);
                        buf->cursor[buf->tos_index] += sizeof(uint16_t);
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }
                break;

            /* 32-bit unsigned int */
            case 'd': {
                    uint32_t *dest = va_arg(va, uint32_t*);
                    if(buf_end(buf) - buf_cursor(buf) >= sizeof(uint32_t)) {
                        *dest = (buf->data[buf->cursor[buf->tos_index + 0]])       +
                                (buf->data[buf->cursor[buf->tos_index + 1]] << 8)  +
                                (buf->data[buf->cursor[buf->tos_index + 2]] << 16) +
                                (buf->data[buf->cursor[buf->tos_index + 3]] << 24);
                        last_int_val = (uint64_t)(*dest);
                        buf->cursor[buf->tos_index] += sizeof(uint32_t);
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }
                break;

            /* 64-bit unsigned int */
            case 'q': {
                    uint64_t *dest = va_arg(va, uint64_t*);
                    if(buf_end(buf) - buf_cursor(buf) >= sizeof(uint64_t)) {
                        *dest = (buf->data[buf->cursor[buf->tos_index]])           +
                                (buf->data[buf->cursor[buf->tos_index + 1]] << 8)  +
                                (buf->data[buf->cursor[buf->tos_index + 2]] << 16) +
                                (buf->data[buf->cursor[buf->tos_index + 3]] << 24) +
                                (buf->data[buf->cursor[buf->tos_index + 4]] << 32) +
                                (buf->data[buf->cursor[buf->tos_index + 5]] << 40) +
                                (buf->data[buf->cursor[buf->tos_index + 6]] << 48) +
                                (buf->data[buf->cursor[buf->tos_index + 7]] << 56);
                        last_int_val = (uint64_t)(*dest);
                        buf->cursor[buf->tos_index] += sizeof(uint64_t);
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }
                break;

            /* 'c' = counted string.  'p' = padded, counted string. */
            case 'c':
            case 'p': {
                    /* we get the size of the count word from the previous field. */
                    uint16_t *str_len = va_arg(va, uint16_t*);
                    uint16_t pad = 0;
                    /* FIXME - need to check for overflow */
                    switch(last_fmt_char) {
                        case 'b':
                        case 'w':
                        case 'W':
                        case 'd':
                        case 'D':
                        case 'q':
                        case 'Q':
                            *str_len = (uint16_t)last_int_val;
                            break;

                        default:
                            info("WARN: previous format field must the the count word but was '%c'.", last_fmt_char);
                            rc = BUF_ERR_UNSUPPORTED_FMT;
                            break;
                    }

                    pad = (fmt[i] == 'p' && (*str_len & 0x01) ? 1 : 0);
                    if(rc == BUF_OK && buf_end(buf) - buf_cursor(buf) >= (*str_len + pad)) {
                        const char **str_data_ptr = va_arg(va, const char **);
                        *str_data_ptr = &(buf->data[buf->cursor[buf->tos_index]]);
                        buf->cursor[buf->tos_index] += last_int_val + pad;
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* nul-terminated C-style string. */
            case 'z': {
                    uint16_t str_length = 0;
                    const char **str_data_ptr = va_arg(va, const char **);

                    *str_data_ptr = &(buf->data[buf->cursor[buf->tos_index]]);

                    str_length = strlen(*str_data_ptr);
                    if(buf_end(buf) - buf_cursor(buf) >= str_length + 1) {
                        /* step past the string. Add one for the terminating nul character. */
                        buf->cursor[buf->tos_index] += str_length + 1;
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            default:
                info("WARN: Unsupported format type '%c'!", fmt[i]);
                rc = BUF_ERR_UNSUPPORTED_FMT;
                break;
        }

        last_fmt_char = fmt[i];
    }

    va_end(va);

    return rc;
}



int32_t buf_pack(buf_t *buf, const char *fmt, ...)
{
    int32_t rc = BUF_OK;
    va_list va;
    uint32_t size_of_fmt_string = 0;
    char last_fmt_char = ' ';
    uint64_t last_int_val = 0;

    if(!fmt) {
        return BUF_ERR_NULL_PTR;
    }

    va_start(va,fmt);

    size_of_fmt_string = strlen(fmt);

    for(uint32_t i=0; i<size_of_fmt_string && rc == BUF_OK; i++) {
        switch(fmt[i]) {
            /* 8-bit unsigned int */
            case 'b': {
                    uint8_t val = va_arg(va, uint8_t);
                    if(buf_cursor(buf) < buf_end(buf)) {
                        buf->data[buf->cursor[buf->tos_index]] = val;
                        buf->cursor[buf->tos_index]++;
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* 16-bit unsigned int */
            case 'w': {
                    uint16_t val = va_arg(va, uint16_t);
                    if(buf_end(buf) - buf_cursor(buf) >= sizeof(uint16_t)) {
                        buf->data[buf->cursor[buf->tos_index + 0]] = (uint8_t)(val & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 1]] = (uint8_t)((val >> 8) & 0xFF);
                        buf->cursor[buf->tos_index] += sizeof(uint16_t);
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* 32-bit unsigned int */
            case 'd': {
                    uint32_t val = va_arg(va, uint32_t);
                    if(buf_end(buf) - buf_cursor(buf) >= sizeof(uint32_t)) {
                        buf->data[buf->cursor[buf->tos_index + 0]] = (uint8_t)(val & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 1]] = (uint8_t)((val >> 8) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 2]] = (uint8_t)((val >> 16) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 3]] = (uint8_t)((val >> 24) & 0xFF);
                        buf->cursor[buf->tos_index] += sizeof(uint32_t);
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* 64-bit unsigned int */
            case 'q': {
                    uint64_t val = va_arg(va, uint64_t);
                    if(buf_end(buf) - buf_cursor(buf) >= sizeof(uint64_t)) {
                        buf->data[buf->cursor[buf->tos_index + 0]] = (uint8_t)(val & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 1]] = (uint8_t)((val >> 8) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 2]] = (uint8_t)((val >> 16) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 3]] = (uint8_t)((val >> 24) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 4]] = (uint8_t)((val >> 32) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 5]] = (uint8_t)((val >> 40) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 6]] = (uint8_t)((val >> 48) & 0xFF);
                        buf->data[buf->cursor[buf->tos_index + 7]] = (uint8_t)((val >> 56) & 0xFF);
                        buf->cursor[buf->tos_index] += sizeof(uint64_t);
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
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
                    if(buf_end(buf) - buf_cursor(buf) >= str_len + pad) {
                        /* copy the string data */
                        for(uint16_t i=0; i<str_len; i++) {
                            buf->data[buf->cursor[buf->tos_index]] = (uint8_t)str_data[i];
                            buf->cursor[buf->tos_index]++;
                        }

                        if(pad) {
                            buf->data[buf->cursor[buf->tos_index]] = (uint8_t)0;
                            buf->cursor[buf->tos_index]++;
                        }
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            /* nul-terminated C-style string */
            case 'z': {
                    /*  */
                    const char *str_data = va_arg(va, const char *);
                    uint16_t str_len = strlen(str_data);

                    /* enough space? */
                    if(buf_end(buf) - buf_cursor(buf) >= str_len + 1) {
                        /* copy the string data */
                        for(uint16_t i=0; i<str_len; i++) {
                            buf->data[buf->cursor[buf->tos_index]] = (uint8_t)str_data[i];
                            buf->cursor[buf->tos_index]++;
                        }

                        /* zero terminate it. */
                        buf->data[buf->cursor[buf->tos_index]] = (uint8_t)0;
                        buf->cursor[buf->tos_index]++;
                    } else {
                        rc = BUF_ERR_INSUFFICIENT_DATA;
                    }
                }

                break;

            default:
                info("WARN: Unsupported format type '%c'!", fmt[i]);
                rc = BUF_ERR_UNSUPPORTED_FMT;
                break;
        }
    }

    return rc;
}
