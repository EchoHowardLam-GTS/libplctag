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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "slice.h"
#include "debug.h"



static inline bool align_offset(slice_p slice, uint8_t alignment)
{
    uint32_t slice_offset = slice_get_offset(slice);
    uint32_t remainder = (alignment ? slice_offset % alignment : 0);

    if(remainder != 0) {
        return slice_set_offset(slice, slice_offset + alignment - remainder);
    }

    return false;
}





slice_val_process_status_t slice_val_decode(struct slice_val_def_t *val_def, slice_p src_slice, slice_p dest_slice)
{
    slice_val_process_status_t rc = SLICE_VAL_PROCESS_STATUS_OK;

    do {
        if(!val_def) {
            warn("Value definition pointer was NULL!");
            rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(!src_slice) {
            warn("Source slice pointer was NULL!");
            rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(!dest_slice) {
            warn("Value destination pointer was NULL!");
            rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(val_def->decoder_func) {
            rc = val_def->decoder_func(val_def, src_slice, dest_slice);
            break;
        } else {
            if(!align_offset(src_slice, val_def->encoded_alignment)) {
                warn("Unable to align source offset!");
                rc = SLICE_VAL_PROCESS_ERR_ALIGN;
                break;
            }

            if(!align_offset(dest_slice, val_def->decoded_alignment)) {
                warn("Unable to align source offset!");
                rc = SLICE_VAL_PROCESS_ERR_ALIGN;
                break;
            }

            if(val_def->val_type == SLICE_VAL_TYPE_ARRAY) {
                detail("Decoding array.");

                /* loop over the elements */
                for(uint32_t index = 0; index < val_def->array_val.num_elements && rc == SLICE_VAL_PROCESS_STATUS_OK; index++) {
                    rc = slice_val_decode(&(val_def->array_val.element_type_def), src_slice, dest_slice);
                    if(rc != SLICE_VAL_PROCESS_STATUS_OK) {
                        warn("Error decoding element %"PRIu32" of an array!", index);
                        break;
                    }
                }
            } else if(val_def->val_type == SLICE_VAL_TYPE_STRUCT) {
                detail("Decoding structure.");

                /* loop over the elements */
                for(uint32_t index = 0; index < val_def->struct_val.num_fields && rc == SLICE_VAL_PROCESS_STATUS_OK; index++) {
                    rc = slice_val_decode(&(val_def->struct_val.field_type_defs[index]), src_slice, dest_slice);
                    if(rc != SLICE_VAL_PROCESS_STATUS_OK) {
                        warn("Error decoding field %"PRIu32" of a structure!", index);
                        break;
                    }
                }
            } else {
                warn("Value definition of type %d has no decoder function!");
                rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            }
        }
    } while(0);

    return rc;
}


slice_val_process_status_t slice_val_encode(struct slice_val_def_t *val_def, slice_p src_slice, slice_p dest_slice)
{
    slice_val_process_status_t rc = SLICE_VAL_PROCESS_STATUS_OK;

    do {
        if(!val_def) {
            warn("Value definition pointer was NULL!");
            rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(!src_slice) {
            warn("Source value pointer was NULL!");
            rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(!dest_slice) {
            warn("Slice destination pointer was NULL!");
            rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(val_def->encoder_func) {
            rc = val_def->encoder_func(val_def, src_slice, dest_slice);
            break;
        } else {
            /* correct alignment */
            uint32_t src_offset = slice_get_offset(src_slice);
            uint8_t src_alignment = val_def->encoded_alignment;
            uint32_t dest_offset = slice_get_offset(dest_slice);
            uint8_t dest_alignment = val_def->decoded_alignment;

            src_offset = align_offset(src_offset, src_alignment);

            if(!slice_set_offset(src_slice, src_offset)) {
                warn("Unable to align source offset!");
                break;
            }

            dest_offset = align_offset(dest_offset, dest_alignment);

            if(!slice_set_offset(dest_slice, dest_offset)) {
                warn("Unable to align destination offset!");
                break;
            }

            if(val_def->val_type == SLICE_VAL_TYPE_ARRAY) {
                /* loop over the elements */
                for(uint32_t index = 0; index < val_def->array_val.num_elements && rc == SLICE_VAL_PROCESS_STATUS_OK; index++) {
                    rc = slice_val_encode(&(val_def->array_val.element_type_def), src_slice, dest_slice);
                    if(rc != SLICE_VAL_PROCESS_STATUS_OK) {
                        warn("Error encoding element %"PRIu32" of an array!", index);
                        break;
                    }
                }
            } else if(val_def->val_type == SLICE_VAL_TYPE_STRUCT) {
                /* loop over the elements */
                for(uint32_t index = 0; index < val_def->struct_val.num_fields && rc == SLICE_VAL_PROCESS_STATUS_OK; index++) {
                    rc = slice_val_encode(&(val_def->struct_val.field_type_defs[index]), src_slice, dest_slice);
                    if(rc != SLICE_VAL_PROCESS_STATUS_OK) {
                        warn("Error encoding field %"PRIu32" of a structure!", index);
                        break;
                    }
                }
            } else {
                warn("Value definition of type %d has no decoder function!");
                rc = SLICE_VAL_PROCESS_ERR_NULL_PTR;
            }
        }
    } while(0);

    return rc;
}


/* basic scalar decoders/encoders */

slice_val_process_status_t slice_decode_u8(struct slice_val_def_t *val_def, slice_p src_slice, slice_p dest_slice)
{
    slice_val_process_status_t rc = SLICE_VAL_PROCESS_STATUS_OK;

    do {
        if(val_def->val_type != SLICE_VAL_TYPE_SCALAR) {
            warn("Expected scalar value type, but got %d type instead!", val_def->val_type);
            rc = SLICE_VAL_PROCESS_ERR_WRONG_TYPE;
            break;
        }

        if(slice_len_from_offset(src_slice) < val_def->encoded_byte_size) {
            warn("Insufficient data left in source slice!");
            rc = SLICE_VAL_PROCESS_ERR_INSUFFICIENT_DATA;
            break;
        }

        if(slice_len_from_offset(dest_slice) < val_def->decoded_byte_size) {
            warn("Insufficient space left in destination slice!");
            rc = SLICE_VAL_PROCESS_ERR_INSUFFICIENT_SPACE;
            break;
        }

        src_val = slice_get_byte_by_offset(src_slice,
                                           slice_get_offset_as_offset(src_slice) + val_def->scalar_val.encoded_byte_offset);
        if(src_val == SLICE_GET_BYTE_FAIL) {
            warn("Unable to get byte from source slice!");
            rc = SLICE_VAL_PROCESS_ERR_INSUFFICIENT_DATA;
            break;
        }

        if(! slice_set_byte(dest_slice,(uint8_t)src_val)) {
            warn("Unable to set byte in destination slice!");
            rc = SLICE_VAL_PROCESS_ERR_PUT;
            break;
        }

        if(! slice_set_offset_by_offset(src_slice, src_start_offset + val_def->scalar_val.encoded_byte_size)) {
            warn("Unable to set offset in source slice!");
            rc = SLICE_VAL_PROCESS_ERR
        }

        if(! slice_set_offset_by_offset(src_slice, dest_start_offset + val_def->scalar_val.decoded_byte_size)) {
            warn("Unable to set offset in source slice!");
        }
    } while(0);

    return rc;
}


slice_val_process_status_t slice_val_encode_u8(struct slice_val_def_t *val_def, void *val_src_arg, slice_p slice_dest)
{
    slice_val_process_status_t rc = SLICE_VAL_PROCESS_STATUS_OK;

    do {

    } while(0);

    return rc;
}
