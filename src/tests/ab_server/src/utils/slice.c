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

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "debug.h"
#include "slice.h"
#include "util.h"


static slice_status_t fix_up_alignment(char alignment_char, slice_p full_data_slice, slice_p unprocessed_data_slice);
static slice_status_t decode_signed_int(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, void *dest);
static slice_status_t encode_signed_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, slice_p unprocessed_data);
static slice_status_t decode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, void *data);
static slice_status_t encode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, slice_p unprocessed_data);
static slice_status_t encode_unsigned_int_impl(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, uint64_t val_arg);
static slice_status_t decode_float(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, void *data);
static slice_status_t encode_float(const char int_size_char, bool little_endian, bool word_swap, va_list va, slice_p unprocessed_data);
static slice_status_t decode_counted_byte_string(const char int_size_char, bool little_endian, bool word_swap, uint32_t multiplier, slice_p unprocessed_data, slice_p dest);
static slice_status_t encode_counted_byte_string(const char int_size_char, bool little_endian, bool word_swap, bool byte_swap, uint32_t multiplier, slice_p unused_data_slice, slice_p src);
static slice_status_t decode_terminated_byte_string(slice_p fmt_slice, slice_p unprocessed_data, slice_p dest);
static slice_status_t encode_terminated_byte_string(slice_p fmt_slice, bool byte_swap, slice_p unused_data_slice, slice_p src);
static slice_status_t check_or_allocate(slice_p slice, uint32_t required_size);
uint32_t *get_byte_order(size_t size, bool little_endian, bool word_swap);



slice_status_t slice_split_by_ptr(slice_p source, uint8_t split_point, slice_p first, slice_p second)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        if(!source) {
            rc = SLICE_ERR_NULL_PTR;
            break;
        }

        if(!split_point) {
            rc = SLICE_ERR_NULL_PTR;
            break;
        }

        if(!first) {
            rc = SLICE_ERR_NULL_PTR;
            break;
        }

        if(!second) {
            rc = SLICE_ERR_NULL_PTR;
            break;
        }

        if(! slice_contains(source, split_point)) {
            rc = SLICE_ERR_OUT_OF_BOUNDS;
            break;
        }

        first->start = source->start;
        first->end = split_point;

        second->start = split_point;
        second->end = source->end;
    } while(0);

    return rc;
}




slice_status_t slice_to_string(slice_p slice, char *result, uint32_t result_size, bool byte_swap)
{
    slice_status_t rc = SLICE_STATUS_OK;
    uint32_t slice_length = 0;
    uint32_t str_length_required = 0;

    do {
        if(!slice) {
            warn("Slice pointer must not be NULL!");
            rc =  SLICE_ERR_NULL_PTR;
            break;
        }

        slice_length = slice_len(slice);

        if(!result) {
            warn("Result pointer must not be NULL!");
            rc =  SLICE_ERR_NULL_PTR;
            break;
        }

        str_length_required += slice_length + 1; /* +1 for the nul terminator */

        /* if we are word swapping we need to have an even number of bytes */
        if(byte_swap && (slice_length & 0x01)) {
            str_length_required += 1;
        }

        if(str_length_required > result_size) {
            warn("Slice contains more data than can fit in the string buffer!");
            rc = SLICE_ERR_TOO_MUCH_DATA;
            break;
        }

        for(uint32_t i=0; i < result_size; i++) {
            uint32_t index = i;

            if(! byte_swap) {
                index = i;
            } else {
                index = (i & 0x01) ? (i - 1) : (i + 1);
            }

            if(i < slice_length) {
                result[i] = *(char *)(slice->start + index);
            } else {
                result[i] = 0;
            }
        }
    } while(0);

    return rc;
}


slice_status_t string_to_slice(const char *source, slice_p dest, bool byte_swap)
{
    slice_status_t rc = SLICE_STATUS_OK;
    uint32_t slice_length = 0;
    uint32_t slice_length_required = 0;
    uint32_t str_length = 0;

    do {
        if(!source) {
            warn("Source pointer must not be NULL!");
            rc =  SLICE_ERR_NULL_PTR;
            break;
        }

        str_length = strlen(source);

        if(!dest) {
            warn("Slice pointer must not be NULL!");
            rc =  SLICE_ERR_NULL_PTR;
            break;
        }

        slice_length = slice_len(dest);

        slice_length_required = str_length;

        /* if we are word swapping we need to have an even number of bytes */
        if(byte_swap && (slice_length_required & 0x01)) {
            slice_length_required += 1;
        }

        if(slice_length_required > slice_len(dest)) {
            warn("Insufficient space in the destination slice!");
            rc = SLICE_ERR_TOO_LITTLE_SPACE;
            break;
        }

        for(uint32_t i=0; i < slice_length_required; i++) {
            uint32_t index = i;

            if(! byte_swap) {
                index = i;
            } else {
                index = (i & 0x01) ? (i - 1) : (i + 1);
            }

            if(i < str_length) {
                *(char *)(dest->start + index) = source[i];
            } else {
                /* zero pad if we need to */
                *(char *)(dest->start + index) = 0;
            }
        }

        dest->end = dest->start + slice_length_required;
    } while(0);

    return rc;
}




slice_status_t slice_from_slice(slice_p parent, slice_p new_slice, uint8_t *start, uint8_t *end)
{
    intptr_t parent_start = 0;
    intptr_t parent_end = 0;
    intptr_t new_slice_start = 0;
    intptr_t new_slice_end = 0;

    if(!parent) {
        warn("Parent pointer cannot be NULL!");
        return SLICE_ERR_NULL_PTR;
    }

    if(!new_slice) {
        warn("New slice pointer cannot be NULL!");
        return SLICE_ERR_NULL_PTR;
    }

    if(!start) {
        warn("Start pointer cannot be NULL!");
        return SLICE_ERR_NULL_PTR;
    }

    if(!end) {
        warn("End pointer cannot be NULL!");
        return SLICE_ERR_NULL_PTR;
    }

    parent_start = (intptr_t)(parent->start);
    parent_end = (intptr_t)(parent->end);
    new_slice_start = (intptr_t)(start);
    new_slice_end = (intptr_t)(end);

    if(parent_start > new_slice_start || new_slice_start > parent_end) {
        warn("Start must be inside the parent slice bounds!");
        return SLICE_ERR_OUT_OF_BOUNDS;
    }

    if(parent_start > new_slice_end || new_slice_end > parent_end) {
        warn("End must be inside the parent slice bounds!");
        return SLICE_ERR_OUT_OF_BOUNDS;
    }

    if(new_slice_start > new_slice_end) {
        warn("Start must be a lower address than end!");
        return SLICE_ERR_OUT_OF_BOUNDS;
    }

    new_slice->start = start;
    new_slice->end = end;

    return SLICE_STATUS_OK;
}




slice_status_t slice_unpack(slice_p data_slice, const char *fmt, ...)
{
    slice_status_t rc = SLICE_STATUS_OK;
    va_list va;
    slice_t fmt_slice = {0};
    slice_t unprocessed_data_slice = {0};
    size_t fmt_len = 0;
    bool little_endian = true;
    bool word_swap = false;
    bool byte_swap = false;

    if(!data_slice || !fmt) {
        warn("Arguments cannot be NULL!");
        return SLICE_ERR_NULL_PTR;
    }

    if(slice_len(data_slice) == 0) {
        warn("Not enough data to decode!");
        return SLICE_ERR_TOO_LITTLE_DATA;
    }

    if((fmt_len = strlen(fmt)) == 0) {
        warn("Format string must not be zero length!");
        return SLICE_ERR_TOO_LITTLE_DATA;
    }

    slice_init(&fmt_slice, fmt, fmt + fmt_len + 1); /* +1 to get the nul terminator. */
    unprocessed_data_slice = *data_slice;

    va_start(va, fmt);

    for(rc = SLICE_STATUS_OK; rc == SLICE_STATUS_OK && slice_len(&fmt_slice) && slice_len(&unprocessed_data_slice); fmt_slice.start++) {
        switch(*(char *)(fmt_slice.start)) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F': {
                    /* insert or match byte value */
                    char hex_digits[3] = {0};
                    char *hex_end = NULL;
                    uint8_t byte_value  = 0;

                    if(slice_len(&fmt_slice) <= 1) {
                        warn("Two hex digits are required for the terminating byte value!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                        break;
                    }

                    hex_digits[0] = *(char *)(fmt_slice.start);
                    fmt_slice.start++;
                    hex_digits[1] = *(char *)(fmt_slice.start);
                    hex_digits[2] = 0;

                    if(!is_hex(hex_digits[0]) || !is_hex(hex_digits[1])) {
                        warn("Expected two hex digits for the byte value but got \"%c%c\"!", hex_digits[0], hex_digits[1]);
                        rc = SLICE_ERR_INCORRECT_FORMAT;
                        break;
                    }

                    byte_value = (uint8_t)strtol(hex_digits, &hex_end, 16);

                    if(hex_digits == hex_end) {
                        warn("strtol() unable to process hex digits!");
                        rc = SLICE_ERR_INCORRECT_FORMAT;
                        break;
                    }

                    /* does the byte in the input buffer match the value here? */
                    if(*(unprocessed_data_slice.start) != byte_value) {
                        warn("Byte value 0x%02x did not match input data byte 0x%02x!", byte_value, *(unprocessed_data_slice.start));
                        rc = SLICE_ERR_NOT_FOUND;
                        break;
                    }
                }
                break;

            case ',': break; /* ignore, used for visual separation in format string. */

            case '<':
                little_endian = true;
                break;

            case '>':
                little_endian = false;
                break;

            case '~': {
                    fmt_slice.start++;

                    if(!slice_len(&fmt_slice)) {
                        warn("Missing swap type in format!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                        break;
                    }

                    if(*(char *)(fmt_slice.start) == 'b') {
                        byte_swap = true;
                    } else if(*(char *)(fmt_slice.start) == 'b') {
                        word_swap = true;
                    } else {
                        warn("Unsupported swap type '%c'!", *(char *)(fmt_slice.start));
                        rc = SLICE_ERR_INCORRECT_FORMAT;
                        break;
                    }
                }

                break;

            case '^': {
                    uint8_t **data_ptr_ptr = va_arg(va, uint8_t **);

                    if(data_ptr_ptr) {
                        *data_ptr_ptr = unprocessed_data_slice.start;
                    } else {
                        rc = SLICE_ERR_NULL_PTR;
                    }
                }
                break;

            case '|': {
                    slice_p first = va_arg(va, slice_p);
                    slice_p second = va_arg(va, slice_p);

                    first->start = data_slice->start;
                    first->end = unprocessed_data_slice.start;

                    second->start = unprocessed_data_slice.start;
                    second->end = unprocessed_data_slice.end;
                }
                break;

            case 'a': {
                    /* skip field type char and point to alignment value */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        char alignment_char = *(char *)(fmt_slice.start);

                        rc = fix_up_alignment(alignment_char, data_slice, &unprocessed_data_slice);
                    } else {
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'i': {
                    /* skip over field type char and point to integer size char */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        void *dest_ptr = va_arg(va, void *);
                        rc = decode_signed_int(*(char *)(fmt_slice.start), little_endian, word_swap, &unprocessed_data_slice, dest_ptr);
                    } else {
                        warn("Format string missing size for integer field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'u': {
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        void *dest_ptr = va_arg(va, void *);
                        rc = decode_unsigned_int(*(char *)(fmt_slice.start), little_endian, word_swap, &unprocessed_data_slice, dest_ptr);
                    } else {
                        warn("Format string missing size for unsigned integer field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'f': {
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        void *dest_ptr = va_arg(va, void *);
                        rc = decode_float(*(char *)(fmt_slice.start), little_endian, word_swap, &unprocessed_data_slice, dest_ptr);
                    } else {
                        warn("Format missing size for float field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'c': {
                    /* step past the 'c' */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice) >= 1) {
                        slice_p dest = va_arg(va, slice_p);
                        rc = decode_counted_byte_string(*(char *)(fmt_slice.start), little_endian, word_swap, 1, &unprocessed_data_slice, dest);
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 't': {
                    /* step past the 't' */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice) >= 1) {
                        slice_p dest = va_arg(va, slice_p);
                        rc = decode_terminated_byte_string(&fmt_slice, &unprocessed_data_slice, dest);
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'e': {
                    slice_p dest = va_arg(va, slice_p);

                    rc = decode_counted_byte_string('1', little_endian, word_swap, 2, &unprocessed_data_slice, dest);
                }

                word_swap = false;
                byte_swap = false;

                break;

            default:
                warn("Unsupported format character '%c'!", *(char *)(fmt_slice.start));
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    }

    va_end(va);

    return rc;
}






slice_status_t slice_pack(slice_t *data_slice, const char *fmt, ...)
{
    slice_status_t rc = SLICE_STATUS_OK;
    va_list va;
    slice_t fmt_slice = {0};
    slice_t unused_data_slice = {0};
    size_t fmt_len = 0;
    bool little_endian = true;
    bool word_swap = false;
    bool byte_swap = false;

    if(!data_slice || !fmt) {
        warn("Arguments cannot be NULL!");
        return SLICE_ERR_NULL_PTR;
    }

    if(slice_len(data_slice) == 0) {
        warn("Not enough space to encode data!");
        return SLICE_ERR_TOO_LITTLE_DATA;
    }

    if((fmt_len = strlen(fmt)) == 0) {
        warn("Format string must not be zero length!");
        return SLICE_ERR_TOO_LITTLE_DATA;
    }

    slice_init(&fmt_slice, fmt, fmt + fmt_len);
    unused_data_slice = *data_slice;

    va_start(va, fmt);

    for(rc = SLICE_STATUS_OK; rc == SLICE_STATUS_OK && slice_len(&fmt_slice) && slice_len(&unused_data_slice); fmt_slice.start++) {
        switch(*(char *)(fmt_slice.start)) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F': {
                    /* insert or match byte value */
                    char hex_digits[3] = {0};
                    char *hex_end = NULL;
                    uint8_t byte_value  = 0;

                    if(slice_len(&fmt_slice) <= 1) {
                        warn("Two hex digits are required for the terminating byte value!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                        break;
                    }

                    hex_digits[0] = *(char *)(fmt_slice.start);
                    fmt_slice.start++;
                    hex_digits[1] = *(char *)(fmt_slice.start);
                    hex_digits[2] = 0;

                    if(!is_hex(hex_digits[0]) || !is_hex(hex_digits[1])) {
                        warn("Expected two hex digits for the byte value but got \"%c%c\"!", hex_digits[0], hex_digits[1]);
                        rc = SLICE_ERR_INCORRECT_FORMAT;
                        break;
                    }

                    byte_value = (uint8_t)strtol(hex_digits, &hex_end, 16);

                    if(hex_digits == hex_end) {
                        warn("strtol() unable to process hex digits!");
                        rc = SLICE_ERR_INCORRECT_FORMAT;
                        break;
                    }

                    /* Insert the byte value */
                    *(unused_data_slice.start) = byte_value;

                    unused_data_slice.start++;
                }
                break;

            case ',': break; /* ignore, used for visual separation in format string. */

            case '<':
                little_endian = true;
                break;

            case '>':
                little_endian = false;
                break;

            case '~': {
                    fmt_slice.start++;

                    if(!slice_len(&fmt_slice)) {
                        warn("Missing swap type in format!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                        break;
                    }

                    if(*(char *)(fmt_slice.start) == 'b') {
                        byte_swap = true;
                    } else if(*(char *)(fmt_slice.start) == 'b') {
                        word_swap = true;
                    } else {
                        warn("Unsupported swap type '%c'!", *(char *)(fmt_slice.start));
                        rc = SLICE_ERR_INCORRECT_FORMAT;
                        break;
                    }
                }
                break;

            case '^': {
                    uint8_t **data_ptr_ptr = va_arg(va, uint8_t **);

                    if(data_ptr_ptr) {
                        *data_ptr_ptr = unused_data_slice.start;
                    } else {
                        rc = SLICE_ERR_NULL_PTR;
                    }
                }
                break;

            case '|': {
                    slice_p first = va_arg(va, slice_p);
                    slice_p second = va_arg(va, slice_p);

                    first->start = data_slice->start;
                    first->end = unused_data_slice.start;

                    second->start = unused_data_slice.start;
                    second->end = unused_data_slice.end;
                }
                break;

            case 'a': {
                    /* skip field type char and point to alignment value */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        char alignment_char = *(char *)(fmt_slice.start);

                        rc = fix_up_alignment(alignment_char, data_slice, &unused_data_slice);
                    } else {
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'i': {
                    /* skip over field type char and point to integer size char */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        rc = encode_signed_int(*(char *)(fmt_slice.start), little_endian, word_swap, va, &unused_data_slice);
                    } else {
                        warn("Format string missing size for integer field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'u': {
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        rc = encode_unsigned_int(*(char *)(fmt_slice.start), little_endian, word_swap, va, &unused_data_slice);
                    } else {
                        warn("Format string missing size for unsigned integer field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'f': {
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice)) {
                        rc = encode_float(*(char *)(fmt_slice.start), little_endian, word_swap, va, &unused_data_slice);
                    } else {
                        warn("Format missing size for float field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'c': {
                    /* step past the 'c' */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice) >= 1) {
                        slice_p src = va_arg(va, slice_p);
                        rc = encode_counted_byte_string(*(char *)(fmt_slice.start), little_endian, word_swap, byte_swap, 1, &unused_data_slice, src);
                    } else {
                        warn("Format missing size for counted byte string field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 't': {
                    /* step past the 't' */
                    fmt_slice.start++;

                    if(slice_len(&fmt_slice) >= 1) {
                        slice_p src = va_arg(va, slice_p);

                        rc = encode_terminated_byte_string(&fmt_slice, byte_swap, &unused_data_slice, src);
                    } else {
                        warn("Format missing size for terminated byte string field!");
                        rc = SLICE_ERR_INCOMPLETE_FORMAT;
                    }
                }

                word_swap = false;
                byte_swap = false;

                break;

            case 'e': {
                    slice_p src = va_arg(va, slice_p);

                    rc = encode_counted_byte_string('1', little_endian, word_swap, byte_swap, 2, &unused_data_slice, src);
                }

                word_swap = false;
                byte_swap = false;

                break;

            default:
                warn("Unsupported format character '%c'!", *(char *)(fmt_slice.start));
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    }

    va_end(va);

    return rc;
}




/****** helpers ******/


slice_status_t fix_up_alignment(char alignment_char, slice_p data_slice, slice_p unprocessed_data_slice)
{
    slice_status_t rc = SLICE_STATUS_OK;
    uint32_t alignment = 1;

    do {
        uint32_t offset = 0;
        uint32_t offset_remainder = 0;

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

        if(rc != SLICE_STATUS_OK || alignment == 1) {
            break;
        }

        offset = (uint32_t)((intptr_t)(unprocessed_data_slice->start) - (intptr_t)(data_slice->start));
        offset_remainder = offset % alignment;

        if(offset_remainder) {
            uint32_t padding = alignment - offset_remainder;

            unprocessed_data_slice->start += padding;

            if(slice_len(unprocessed_data_slice) == 0) {
                warn("No unprocessed data remaining after alignment!");
                rc = SLICE_ERR_TOO_LITTLE_DATA;
            }
        }
    } while(0);

    return rc;
}


slice_status_t decode_signed_int(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, void *dest)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                int8_t *val = (uint8_t *)dest;

                *val = (int8_t)(*(unprocessed_data->start));

                unprocessed_data->start ++;
            }
            break;

            case '2': {
                    int16_t *val = (int16_t *)dest;
                    uint32_t *byte_order = get_byte_order(sizeof(int16_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(int16_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *val |= (int16_t)((int16_t)(*(unprocessed_data->start)) << (byte_order[count]));
                    }

                    if(count < sizeof(int16_t)) {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '4': {
                    int32_t *val = (int32_t *)dest;
                    uint32_t *byte_order = get_byte_order(sizeof(int32_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(int32_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *val |= (int32_t)((int32_t)(*(unprocessed_data->start)) << (byte_order[count]));
                    }

                    if(count < sizeof(int32_t)) {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '8': {
                    int64_t *val = (int64_t *)dest;
                    uint32_t *byte_order = get_byte_order(sizeof(int64_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(int64_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *val |= (int64_t)((int64_t)(*(unprocessed_data->start)) << (byte_order[count]));
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


slice_status_t encode_signed_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, slice_p unprocessed_data)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                int8_t val = va_arg(va, int8_t);

                *(unprocessed_data->start) = (uint8_t)val;

                unprocessed_data->start++;
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

                    for(count = 0; count < sizeof(int16_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
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

                    for(count = 0; count < sizeof(int32_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
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

                    for(count = 0; count < sizeof(int64_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
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


slice_status_t decode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, void *dest)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                uint8_t *val = (uint8_t *)dest;

                *val = (uint8_t)(*(unprocessed_data->start));

                unprocessed_data->start++;
            }
            break;

            case '2': {
                    uint16_t *val = (uint16_t *)dest;
                    uint32_t *byte_order = get_byte_order(sizeof(uint16_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(uint16_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *val |= (uint16_t)((uint16_t)(*(unprocessed_data->start)) << (byte_order[count]));
                    }

                    if(count < sizeof(uint16_t)) {
                        warn("Insufficient data in buffer to decode unsigned integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '4': {
                    uint32_t *val = (uint32_t *)dest;
                    uint32_t *byte_order = get_byte_order(sizeof(uint32_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(uint32_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *val |= (uint32_t)((uint32_t)(*(unprocessed_data->start)) << (byte_order[count]));
                    }

                    if(count < sizeof(uint32_t)) {
                        warn("Insufficient data in buffer to decode unsigned integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '8': {
                    uint64_t *val = (uint64_t *)dest;
                    uint32_t *byte_order = get_byte_order(sizeof(uint64_t), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    *val = 0;

                    for(count = 0; count < sizeof(uint64_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *val |= (uint64_t)((uint64_t)(*(unprocessed_data->start)) << (byte_order[count]));
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


slice_status_t encode_unsigned_int_impl(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, uint64_t val_arg)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                if(val_arg > UINT8_MAX) {
                    warn("Passed value is too large to fit in a U8!");
                    rc = SLICE_ERR_OVERFLOW;
                    break;
                }

                *(unprocessed_data->start) = (uint8_t)val_arg;

                unprocessed_data->start++;
            }
            break;

            case '2': {
                    uint16_t val = (uint16_t)val_arg;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(val_arg > UINT16_MAX) {
                        warn("Passed value is too large to fine in a U16!");
                        rc = SLICE_ERR_OVERFLOW;
                        break;
                    }

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(val)) {
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;

            case '4': {
                    uint32_t val = (uint32_t)val_arg;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(val_arg > UINT32_MAX) {
                        warn("Passed value is too large to fine in a U16!");
                        rc = SLICE_ERR_OVERFLOW;
                        break;
                    }

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(val)) {
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;

            case '8': {
                    uint64_t val = (uint64_t)val_arg;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(val_arg > UINT64_MAX) {
                        warn("Passed value is too large to fine in a U16!");
                        rc = SLICE_ERR_OVERFLOW;
                        break;
                    }

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(val)) {
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


slice_status_t encode_unsigned_int(const char int_size_char, bool little_endian, bool word_swap, va_list va, slice_p unprocessed_data)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '1': {
                uint8_t val = va_arg(va, uint8_t);

                rc = encode_unsigned_int_impl(int_size_char, little_endian, word_swap, unprocessed_data, (uint64_t)val);
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

                    for(count = 0; count < sizeof(uint16_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
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

                    for(count = 0; count < sizeof(uint32_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
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

                    for(count = 0; count < sizeof(uint64_t) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
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



slice_status_t decode_float(const char int_size_char, bool little_endian, bool word_swap, slice_p unprocessed_data, void *dest)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '4': {
                    float *float_val = (float *)dest;
                    uint32_t val = 0;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    val = 0;

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        val |= (uint32_t)((uint32_t)(*(unprocessed_data->start)) << (byte_order[count]));
                    }

                    if(count == sizeof(val)) {
                        float tmp_float = 0.0;
                        memcpy(&tmp_float, &val, sizeof(float));
                        *float_val = tmp_float;
                    } else {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            case '8': {
                    double *float_val = (double *)dest;
                    uint64_t val = 0;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        val |= (uint64_t)((uint64_t)(*(unprocessed_data->start)) << (byte_order[count]));
                    }

                    if(count == sizeof(val)) {
                        double tmp_double = 0.0;
                        memcpy(&tmp_double, &val, sizeof(tmp_double));

                        *float_val = tmp_double;
                    } else {
                        warn("Insufficient data in buffer to decode integer!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                    }
                }
                break;

            default:
                warn("Unsupported float length character %c", int_size_char);
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    } while(0);

    return rc;
}



slice_status_t encode_float(const char int_size_char, bool little_endian, bool word_swap, va_list va, slice_p unprocessed_data)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        switch(int_size_char) {
            case '4': {
                    float float_val = va_arg(va, float);
                    uint32_t val = 0;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    memcpy(&val, &float_val, sizeof(val));

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(val)) {
                        warn("Insufficient data to decode float!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;

            case '8': {
                    double float_val = va_arg(va, double);
                    uint64_t val = 0;
                    uint32_t *byte_order = get_byte_order(sizeof(val), little_endian, word_swap);
                    size_t count = 0;

                    if(!byte_order) {
                        warn("Byte order array pointer is null!");
                        rc = SLICE_ERR_NULL_PTR;
                        break;
                    }

                    memcpy(&val, &float_val, sizeof(val));

                    for(count = 0; count < sizeof(val) && slice_len(unprocessed_data); unprocessed_data->start++, count++) {
                        *(unprocessed_data->start) = (uint8_t)((val >> ((byte_order[count]) * 8)) & 0xFF);
                    }

                    if(count < sizeof(val)) {
                        warn("Insufficient data to decode float!");
                        rc = SLICE_ERR_TOO_LITTLE_DATA;
                        break;
                    }
                }
                break;

            default:
                warn("Unsupported float length character %c", int_size_char);
                rc = SLICE_ERR_UNSUPPORTED_FORMAT;
                break;
        }
    } while(0);

    return rc;
}



slice_status_t decode_counted_byte_string(const char int_size_char, bool little_endian, bool word_swap, uint32_t count_multiplier, slice_p unprocessed_data, slice_p dest)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        uint64_t count = 0;

        /* get the count value */
        switch(int_size_char) {
            case '1': {
                uint8_t tmp_count = 0;
                rc = decode_unsigned_int(int_size_char, little_endian, word_swap, unprocessed_data, (void*)&tmp_count);
                count = (uint64_t)count_multiplier * (uint64_t)tmp_count;
            }
            break;

            case '2': {
                uint16_t tmp_count = 0;
                rc = decode_unsigned_int(int_size_char, little_endian, word_swap, unprocessed_data, (void*)&tmp_count);
                count = (uint64_t)count_multiplier * (uint64_t)tmp_count;
            }
            break;

            case '4': {
                uint32_t tmp_count = 0;
                rc = decode_unsigned_int(int_size_char, little_endian, word_swap, unprocessed_data, (void*)&tmp_count);
                count = (uint64_t)count_multiplier * (uint64_t)tmp_count;
            }
            break;

            case '8': {
                uint64_t tmp_count = 0;
                rc = decode_unsigned_int(int_size_char, little_endian, word_swap, unprocessed_data, (void*)&tmp_count);
                count = (uint64_t)count_multiplier * (uint64_t)tmp_count;
            }
            break;

            default:
                warn("Unsupported counted byte string length '%c'!", int_size_char);
                rc = SLICE_ERR_INCORRECT_FORMAT;
                break;
        }

        if(rc != SLICE_STATUS_OK) {
            break;
        }

        if(! slice_len(unprocessed_data) < count) {
            warn("Count value, %"PRIu64" is larger than available data!", count);
            rc = SLICE_ERR_TOO_LITTLE_DATA;
            break;
        }

        /* set up the destination slice */
        dest->start = unprocessed_data->start;
        dest->end = unprocessed_data->start + count;

        /* bump the unprocessed data start since we consumed the counted byte string */
        unprocessed_data->start += count;
    } while(0);

    return rc;
}



slice_status_t encode_counted_byte_string(const char int_size_char, bool little_endian, bool word_swap, bool byte_swap, uint32_t divisor, slice_p unused_data_slice, slice_p source_arg)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        uint64_t max_count = 0;
        uint64_t byte_str_length = slice_len(source_arg);
        uint64_t count = 0;
        slice_t source = *source_arg;

        switch(int_size_char) {
            case '1': max_count = UINT8_MAX; break;
            case '2': max_count = UINT16_MAX; break;
            case '4': max_count = UINT32_MAX; break;
            case '8': max_count = UINT64_MAX; break;

            default: max_count = 0; break;
        }

        if(max_count == 0) {
            warn("The size of the count word must be 1, 2, 4, or 8, but was %c!", int_size_char);
            rc = SLICE_ERR_INCORRECT_FORMAT;
            break;
        }

        if(divisor == 0) {
            warn("Divisor must be non-zero!");
            rc = SLICE_ERR_BAD_PARAM;
            break;
        }

        if(byte_str_length % divisor) {
            warn("Slice length, %"PRIu64" is not a multiple of the divisor %"PRIu32"!", byte_str_length, divisor);
            rc = SLICE_ERR_BAD_PARAM;
            break;
        }

        if(byte_swap && (byte_str_length & 0x01)) {
            warn("Slice length, %"PRIu64" is not a multiple of two which is required for byte swapping!!", byte_str_length);
            rc = SLICE_ERR_BAD_PARAM;
            break;
        }

        count = byte_str_length / divisor;

        if(count > max_count) {
            warn("Source slice contains more data than can fit in a two-byte count word!");
            rc = SLICE_ERR_TOO_MUCH_DATA;
            break;
        }

        /* write out the count word. */
        rc = encode_unsigned_int_impl(int_size_char, little_endian, word_swap, unused_data_slice, count);
        if(rc != SLICE_STATUS_OK) {
            break;
        }

        if(byte_str_length > slice_len(unused_data_slice)) {
            warn("Insufficient space in encoding buffer to copy byte string!");
            rc = SLICE_ERR_TOO_LITTLE_SPACE;
            break;
        }

        for(uint64_t i = 0; i < byte_str_length; i++) {
            uint32_t index = i;

            if(! byte_swap) {
                index = i;
            } else {
                index = (i & 0x01) ? (i - 1) : (i + 1);
            }

            *(unused_data_slice->start + index) = source.start[i];
        }

        /* consume the unused space */
        unused_data_slice->start += byte_str_length;
    } while(0);

    return rc;
}



slice_status_t decode_terminated_byte_string(slice_p fmt_slice, slice_p unprocessed_data, slice_p dest)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        uint8_t terminator = 0;
        char *hex_end = NULL;
        char hex_digits[3] = {0};
        slice_t byte_str_source_slice = {0};

        if(slice_len(fmt_slice) <= 1) {
            warn("Two hex digits are required for the terminating byte value!");
            rc = SLICE_ERR_INCOMPLETE_FORMAT;
            break;
        }

        hex_digits[0] = *(char *)(fmt_slice->start);
        fmt_slice->start++;
        hex_digits[1] = *(char *)(fmt_slice->start);
        hex_digits[2] = 0;

        if(!is_hex(hex_digits[0]) || !is_hex(hex_digits[1])) {
            warn("Expected two hex digits for the terminating byte value but got \"%c%c\"!", hex_digits[0], hex_digits[1]);
            rc = SLICE_ERR_INCORRECT_FORMAT;
            break;
        }

        terminator = (uint8_t)strtol(hex_digits, &hex_end, 16);

        if(hex_digits == hex_end) {
            warn("strtol() unable to process hex digits!");
            rc = SLICE_ERR_INCORRECT_FORMAT;
            break;
        }

        /* capture the start pointer.  We'll overwrite the end later. */
        byte_str_source_slice.start = unprocessed_data->start;

        /* find the end of the terminated byte string */
        rc = SLICE_ERR_NOT_FOUND;
        while(slice_len(unprocessed_data)) {
            if(*(char *)(unprocessed_data->start) == terminator) {
                rc = SLICE_STATUS_OK;
                break;
            }
            unprocessed_data->start ++;
        }

        /* did we find a terminator? */
        if(rc != SLICE_STATUS_OK) {
            warn("Could not find terminating byte in data!");
            break;
        }

        /* step past the terminator */
        unprocessed_data->start++;

        /* set up the source slice for the byte string data. */
        byte_str_source_slice.end = unprocessed_data->start;

        /* copy the slice fields */
        *dest = byte_str_source_slice;
    } while(0);

    return rc;
}



slice_status_t encode_terminated_byte_string(slice_p fmt_slice, bool byte_swap, slice_p unused_data_slice, slice_p source_slice)
{
    slice_status_t rc = SLICE_STATUS_OK;

    do {
        uint8_t terminator = 0;
        char *hex_end = NULL;
        char hex_digits[3] = {0};
        uint32_t source_slice_len = slice_len(source_slice);

        if(slice_len(fmt_slice) <= 1) {
            warn("Two hex digits are required for the terminating byte value!");
            rc = SLICE_ERR_INCOMPLETE_FORMAT;
            break;
        }

        hex_digits[0] = *(char *)(fmt_slice->start);
        fmt_slice->start++;
        hex_digits[1] = *(char *)(fmt_slice->start);
        hex_digits[2] = 0;

        if(!is_hex(hex_digits[0]) || !is_hex(hex_digits[1])) {
            warn("Expected two hex digits for the terminating byte value but got \"%c%c\"!", hex_digits[0], hex_digits[1]);
            rc = SLICE_ERR_INCORRECT_FORMAT;
            break;
        }

        terminator = (uint8_t)strtol(hex_digits, &hex_end, 16);

        if(hex_digits == hex_end) {
            warn("strtol() unable to process hex digits!");
            rc = SLICE_ERR_INCORRECT_FORMAT;
            break;
        }

        if((source_slice_len + 1) > slice_len(unused_data_slice)) {
            warn("Insufficient space to copy byte string into encoding buffer!");
            rc = SLICE_ERR_TOO_LITTLE_SPACE;
            break;
        }

        if(byte_swap && ((source_slice_len + 1) & 0x01)) {
            warn("Byte string length must be a multiple of two, including terminator, for byte swapping!");
            rc = SLICE_ERR_BAD_PARAM;
            break;
        }

        /*
         * Intentionally count past the end of the source so that we can add the
         * terminator byte.
         */
        for(uint64_t i = 0; i < (source_slice_len + 1); i++) {
            uint32_t index = 0;
            uint8_t source_val = 0;

            if(i < source_slice_len) {
                source_val = source_slice->start[i];
            } else {
                source_val = terminator;
            }

            if(! byte_swap) {
                index = i;
            } else {
                index = (i & 0x01) ? (i - 1) : (i + 1);
            }

            *(unused_data_slice->start + index) = source_val;
        }

        /* consume the unused space */
        unused_data_slice->start += source_slice_len + 1;
    } while(0);
}






slice_status_t check_or_allocate(slice_p slice, uint32_t required_size)
{
    slice_status_t rc = SLICE_ERR_NULL_PTR;

    if(slice) {
        /* does the slice already point to data */
        if(slice->start) {
            if(slice_len(slice) >= required_size) {
                rc = SLICE_STATUS_OK;
            } else {
                rc = SLICE_ERR_TOO_LITTLE_SPACE;
            }
        } else {
            /* we'll assume that the slice is empty */
            slice->start = calloc(1, required_size);

            assert_error((slice->start), "Unable to allocate memory for slice data buffer!");

            slice->end = slice->start + required_size;

            rc = SLICE_STATUS_OK;
        }
    }

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
