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



bool slice_init_parent(slice_p parent, uint8_t *data, uint32_t data_len)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        if(!parent || !data) {
            rc = STATUS_NULL_PTR;
            break;
        }

        if(data_len > SLICE_MAX_LEN) {
            rc = STATUS_BAD_INPUT;
            break;
        }

        parent->parent = NULL;
        parent->data = data;
        parent->start = 0;
        parent->end = data_len;
    } while(0);

    if(parent) {
        parent->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}



bool slice_init_child(slice_p child, slice_p parent)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        if(!child || !parent) {
            rc = STATUS_NULL_PTR;
            break;
        }

        child->parent = parent;
        child->data = parent->data;
        child->start = parent->start;
        child->end = parent->end;
    } while(0);

    if(child) {
        child->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}




/* set the start to an offset within the range [parent->start, child->end] */
bool slice_set_start(slice_p slice, uint32_t possible_offset)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        uint32_t offset = possible_offset;

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        /* cannot change any bounds if there is no parent. */
        if(!slice->parent) {
            rc = STATUS_NOT_ALLOWED;
            break;
        }

        /* clamp values between parent start and current slice end */
        offset = (offset >= slice->parent->start ? offset : slice->parent->start);
        offset = (offset <= slice->end ? offset : slice->end);

        slice->start = offset;
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}


bool slice_set_start_delta(slice_p slice, int32_t start_delta)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        int64_t new_start = 0;

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        /* cannot change any bounds if there is no parent. */
        if(!slice->parent) {
            rc = STATUS_NOT_ALLOWED;
            break;
        }

        new_start = (int64_t)(slice->start) + (int64_t)(start_delta);

        /* clamp the values to 0..SLICE_MAX_LEN */
        new_start = (new_start >= 0 ? new_start : 0);
        new_start = (new_start < SLICE_MAX_LEN ? new_start : SLICE_MAX_LEN - 1);

        rc = slice_set_start(slice, (uint32_t)new_start);
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}


/* set the end to an offset within the child's start and parent's end */
bool slice_set_end(slice_p slice, uint32_t possible_offset)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        uint32_t offset = possible_offset;

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        /* cannot change any bounds if there is no parent. */
        if(!slice->parent) {
            rc = STATUS_NOT_ALLOWED;
            break;
        }

        /* clamp to boundaries */
        offset = (offset <= slice->parent->end ? offset : slice->parent->end);
        offset = (offset >= slice->start ? offset : slice->start);

        slice->end = offset;
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}


bool slice_set_end_delta(slice_p slice, int32_t end_delta)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        int64_t new_end = 0;

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        /* cannot change any bounds if there is no parent. */
        if(!slice->parent) {
            rc = STATUS_NOT_ALLOWED;
            break;
        }

        new_end = (int64_t)(slice->end) + (int64_t)(end_delta);

        /* clamp to limits */
        new_end = (new_end >= 0 ? new_end : 0);
        new_end = (new_end < SLICE_MAX_LEN ? new_end : SLICE_MAX_LEN - 1);

        rc = slice_set_end(slice, (uint32_t)new_end);
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}


bool slice_set_len(slice_p slice, uint32_t new_len)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        uint64_t new_end = 0;

        if(!slice || !slice->parent) {
            rc = STATUS_OUT_OF_BOUNDS;
            break;
        }

        new_end = (uint64_t)(slice->start) + (uint64_t)(new_len);

        /* clamp to boundaries */
        new_end = (new_end <= (uint64_t)(slice->parent->end)) ? new_end : (uint64_t)(slice->parent->end);

        rc = slice_set_end(slice, (uint32_t)new_end);
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}





/*
 *  Data Accessors
 */

uint8_t byte_order_8[] = { 8, 0 };

uint8_t byte_order_16_le[] = { 16, 0, 1 };
uint8_t byte_order_32_le[] = { 32, 0, 1, 2, 3 };
uint8_t byte_order_64_le[] = { 64, 0, 1, 2, 3, 4, 5, 6, 7 };

uint8_t byte_order_16_be[] = { 16, 1, 0 };
uint8_t byte_order_32_be[] = { 32, 3, 2, 1, 0 };
uint8_t byte_order_64_be[] = { 64, 7, 6, 5, 4, 3, 2, 1, 0 };

uint8_t byte_order_32_le_swapped[] = { 32, 2, 3, 0, 1 };
uint8_t byte_order_64_le_swapped[] = { 64, 2, 3, 0, 1, 6, 7, 4, 5 };

uint8_t byte_order_32_be_swapped[] = { 32, 1, 0, 3, 2 };
uint8_t byte_order_64_be_swapped[] = { 64, 5, 4, 7, 6, 1, 0, 3, 2 };

static uint8_t *get_byte_order_array(slice_byte_order_t byte_order, uint8_t num_bits)
{
    uint8_t *result = NULL;

    detail("Starting.");

    switch(num_bits) {
        case 8:
            result = byte_order_8;
            break;

        case 16:
            switch(byte_order) {
                case SLICE_BYTE_ORDER_LE_WORD_SWAP:
                case SLICE_BYTE_ORDER_LE:
                    result = byte_order_16_le;
                    break;

                case SLICE_BYTE_ORDER_BE_WORD_SWAP:
                case SLICE_BYTE_ORDER_BE:
                    result = byte_order_16_be;
                    break;

                default:
                    warn("Unknown byte order value %d found!", byte_order);
                    break;
            }
            break;

        case 32:
            switch(byte_order) {
                case SLICE_BYTE_ORDER_LE:
                    result = byte_order_32_le;
                    break;

                case SLICE_BYTE_ORDER_LE_WORD_SWAP:
                    result = byte_order_32_le_swapped;
                    break;

                case SLICE_BYTE_ORDER_BE:
                    result = byte_order_32_be;
                    break;

                case SLICE_BYTE_ORDER_BE_WORD_SWAP:
                    result = byte_order_32_be_swapped;
                    break;

                default:
                    warn("Unknown byte order value %d found!", byte_order);
                    break;
            }
            break;

        case 64:
            switch(byte_order) {
                case SLICE_BYTE_ORDER_LE:
                    result = byte_order_64_le;
                    break;

                case SLICE_BYTE_ORDER_LE_WORD_SWAP:
                    result = byte_order_64_le_swapped;
                    break;

                case SLICE_BYTE_ORDER_BE:
                    result = byte_order_64_be;
                    break;

                case SLICE_BYTE_ORDER_BE_WORD_SWAP:
                    result = byte_order_64_be_swapped;
                    break;

                default:
                    warn("Unknown byte order value %d found!", byte_order);
                    break;
            }
            break;

        default:
            warn("Unsupported number of bits %d!", num_bits);
            break;
    }

    if(result) {
        detail("Done with valid byte order array.");
    } else {
        detail("Done with no matching byte order array!");
    }

    return result;
}



uint64_t slice_get_uint(slice_p slice, uint32_t offset, slice_byte_order_t byte_order_type, uint8_t num_bits)
{
    status_t rc = STATUS_OK;
    uint64_t result = 0;

    detail("Starting.");

    do {
        uint32_t num_bytes = num_bits / 8;
        uint8_t *byte_order_array = get_byte_order_array(byte_order_type, num_bits);

        detail("Getting unsigned it of %"PRIu8" bits from offset %"PRIu32" to offset %"PRIu32".", num_bits, offset, offset + num_bytes);

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        if(!byte_order_array) {
            rc = STATUS_BAD_INPUT;
            break;
        }

        switch(num_bits) {
            case 8:
                /* FIXME - fold this into the loop below. */
                if(slice_contains_offset(slice, offset)) {
                    result = (uint64_t)(slice->data[offset]);
                }
                break;

            case 16:
            case 32:
            case 64:
                if(!byte_order_array || num_bits != byte_order_array[0]) {
                    warn("No byte array or number of bits %"PRIu8" does not equal the byte order array check entry!", num_bits);

                    rc = STATUS_INTERNAL_FAILURE;

                    break;
                }

                if(slice_contains_offset(slice, offset) && slice_contains_offset(slice, offset + (num_bytes - 1))) {
                    result = 0;

                    for(uint32_t index = 0; index < num_bytes; index++) {
                        result |= (uint64_t)((uint64_t)(slice->data[offset + byte_order_array[1 + index]]) << index);
                    }
                }
                break;

            default:
                warn("Number of bits, %u, is not supported!", num_bits);
                rc = STATUS_BAD_INPUT;
                break;
        }

    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return result;
}



bool slice_set_uint(slice_p slice, uint32_t offset, slice_byte_order_t byte_order_type, uint8_t num_bits, uint64_t val)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        uint32_t num_bytes = num_bits / 8;
        uint8_t *byte_order_array = get_byte_order_array(byte_order_type, num_bits);

        detail("Getting unsigned it of %"PRIu8" bits from offset %"PRIu32" to offset %"PRIu32".", num_bits, offset, offset + num_bytes);

        if(!slice) {
            warn("The target slice must not be NULL!");
            rc = STATUS_NULL_PTR;
            break;
        }

        if(!byte_order_array) {
            warn("Unable to look up the byte order array!");
            rc = STATUS_BAD_INPUT;
            break;
        }

        switch(num_bits) {
            case 8:
                /* FIXME - add the byte order arrays for 8-bit units and fold this into the loop below. */
                if(slice_contains_offset(slice, offset)) {
                    slice->data[offset] = (uint8_t)(val & 0xFF);
                }
                break;

            case 16:
            case 32:
            case 64:
                if(!byte_order_array || num_bits != byte_order_array[0]) {
                    warn("No byte array or number of bits %"PRIu8" does not equal the byte order array check entry!", num_bits);

                    rc = STATUS_INTERNAL_FAILURE;

                    break;
                }

                if(slice_contains_offset(slice, offset) && slice_contains_offset(slice, offset + (num_bytes - 1))) {
                    for(uint32_t index = 0; index < num_bytes; index++) {
                        slice->data[offset + byte_order_array[1 + index]] = (uint8_t)((val >> index) & 0xFF);
                    }
                }
                break;

            default:
                warn("Number of bit, %u, is not supported!", num_bits);
                rc = STATUS_BAD_INPUT;
                break;
        }

    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}


double slice_get_float(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits)
{
    double d_result = 0.0;

    detail("Starting.");

    do {
        uint64_t u_result = 0;

        if(!slice) {
            break;
        }

        if(num_bits != 32 || num_bits != 64) {
            warn("Only 32 and 64-bit floats are supported.");
            slice->status = STATUS_NOT_SUPPORTED;
            break;
        }

        u_result = slice_get_uint(slice, offset, byte_order, num_bits);

        memcpy(&d_result, &u_result, sizeof(d_result));
    } while(0);

    detail("Done with status %s.", status_to_str(slice_get_status(slice)));

    return d_result;
}



bool slice_set_float(slice_p slice, uint32_t offset, slice_byte_order_t byte_order, uint8_t num_bits, double val)
{
    status_t rc = STATUS_OK;

    do {
        uint64_t u_val = 0;

        if(!slice) {
            rc = STATUS_NULL_PTR;
            break;
        }

        if(num_bits != 32 || num_bits != 64) {
            warn("Only 32 and 64-bit floats are supported.");
            slice->status = STATUS_NOT_SUPPORTED;
            break;
        }

        memcpy(&u_val, &val, sizeof(u_val));

        slice_set_uint(slice, offset, byte_order, num_bits, u_val);

        rc = slice_get_status(slice);
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}



bool slice_get_byte_string(slice_p slice, uint32_t byte_str_offset, uint32_t byte_str_len, uint8_t *dest, bool byte_swap)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        if(!slice || !dest) {
            warn("The source slice pointer and the destination byte string pointer must not be NULL!");
            rc = STATUS_NULL_PTR;
            break;
        }

        if(!slice_contains_offset(slice, byte_str_offset) || !slice_contains_offset(slice, byte_str_offset + (byte_str_len - 1))) {
            warn("The slice does not contain all the data requested.");
            rc = STATUS_NO_RESOURCE;
            break;
        }

        if(byte_swap && (byte_str_len & 0x01)) {
            warn("Byte swapped byte strings must have an even length.");
            rc = STATUS_BAD_INPUT;
            break;
        }

        /* copy the data */
        for(uint32_t index = 0; index < byte_str_len; index++) {
            dest[index] = slice->data[byte_str_offset + index];
        }
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}



bool slice_set_byte_string(slice_p slice, uint32_t byte_str_offset, uint8_t *byte_str_src, uint32_t byte_str_len, bool byte_swap)
{
    status_t rc = STATUS_OK;

    detail("Starting.");

    do {
        if(!slice || !byte_str_src) {
            warn("Destination slice and byte string pointer must not be NULL!");
            rc = STATUS_NULL_PTR;
            break;
        }

        if(!slice_contains_offset(slice, byte_str_offset) || !slice_contains_offset(slice, byte_str_offset + (byte_str_len - 1))) {
            warn("The slice does not have enough space for all the byte string data.");
            rc = STATUS_NO_RESOURCE;
            break;
        }

        if(byte_swap && (byte_str_len & 0x01)) {
            warn("Byte swapped byte strings must have an even length.");
            rc = STATUS_BAD_INPUT;
            break;
        }

        /* copy the data */
        for(uint32_t index = 0; index < byte_str_len; index++) {
            slice->data[byte_str_offset + index] = byte_str_src[index];
        }
    } while(0);

    if(slice) {
        slice->status = rc;
    }

    detail("Done with status %s.", status_to_str(rc));

    return (rc == STATUS_OK);
}
