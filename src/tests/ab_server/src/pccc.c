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

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "cip.h"
#include "eip.h"
#include "pccc.h"
#include "plc.h"
#include "buf.h"
#include "utils.h"

const uint8_t PCCC_PREFIX[] = { 0x0f, 0x00 };
const uint8_t PLC5_READ[] = { 0x01 };
const uint8_t PLC5_WRITE[] = { 0x00 };
const uint8_t SLC_READ[] = { 0xa2 };
const uint8_t SLC_WRITE[] = { 0xaa };

const uint8_t PCCC_RESP_PREFIX[] = { 0xcb, 0x00, 0x00, 0x00, 0x07, 0x3d, 0xf3, 0x45, 0x43, 0x50, 0x21 };


// 4f f0 3c 96 06 - address does not point to something usable.
// 4f f0 fa da 07 - file is wrong size.
// 4f f0 a6 b3 0e - command could not be decoded

static int handle_plc5_read_request(tcp_client_p client);
static int handle_plc5_write_request(tcp_client_p client);
static int handle_slc_read_request(tcp_client_p client);
static int handle_slc_write_request(tcp_client_p client);
static int make_pccc_error(tcp_client_p client, uint8_t err_code);


int dispatch_pccc_request(tcp_client_p client)
{
    int rc = PCCC_OK;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    uint16_t pccc_request_size = 0;
    uint16_t response_prefix_offset = 0;

    info("Got packet:");
    buf_dump_offset(request, buf_get_cursor(request));

    pccc_request_size = buf_len(request) - buf_get_cursor(request);

    if(pccc_request_size < 20) { /* FIXME - 13 + 7 */
        info("Packet too short!");
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    // /* copy the response prefix. */
    // for(size_t i=0; i < sizeof(PCCC_RESP_PREFIX); i++) {
    //     buf_set_uint8(response, PCCC_RESP_PREFIX[i]);
    // }

    response_prefix_offset = buf_get_cursor(response);

    buf_set_cursor(response, buf_get_cursor(response) + sizeof(PCCC_RESP_PREFIX));

    if(buf_match_bytes(request, PCCC_PREFIX, sizeof(PCCC_PREFIX))) {
        buf_t pccc_command;

        info("Matched valid PCCC prefix.");

        /* two pad or ignored bytes */
        buf_get_uint8(request);
        buf_get_uint8(request);

        client->conn_config.pccc_seq_id = buf_get_uint16_le(request);

        /* match the command. */
        if(client->plc->plc_type == PLC_PLC5 && buf_match_bytes(request, PLC5_READ, sizeof(PLC5_READ))) {
            rc = handle_plc5_read_request(client);
        } else if(client->plc->plc_type == PLC_PLC5 && buf_match_bytes(request, PLC5_WRITE, sizeof(PLC5_WRITE))) {
            rc = handle_plc5_write_request(client);
        } else if((client->plc->plc_type == PLC_SLC || client->plc->plc_type == PLC_MICROLOGIX) && buf_match_bytes(request, SLC_READ, sizeof(SLC_READ))) {
            rc = handle_slc_read_request(client);
        } else if((client->plc->plc_type == PLC_SLC || client->plc->plc_type == PLC_MICROLOGIX) && buf_match_bytes(request, SLC_WRITE, sizeof(SLC_WRITE))) {
            rc = handle_slc_write_request(client);
        } else {
            info("Unsupported PCCC command!");
            make_pccc_error(client, PCCC_ERR_UNSUPPORTED_COMMAND);
            rc = PCCC_ERR_UNSUPPORTED_COMMAND;
        }
    }

    /* back fill the PCCC response prefix */
    buf_set_cursor(response, response_prefix_offset);

    for(size_t i=0; i < sizeof(PCCC_RESP_PREFIX); i++) {
        buf_set_uint8(response, PCCC_RESP_PREFIX[i]);
    }

    info("PCCC response:");
    buf_dump_offset(response, response_prefix_offset);

    return rc;
}


int handle_plc5_read_request(tcp_client_p client)
{
    int rc = PCCC_OK;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    uint16_t offset = 0;
    size_t start_byte_offset = 0;
    uint16_t transfer_size = 0;
    size_t end_byte_offset = 0;
    size_t tag_size = 0;
    size_t data_file_num = 0;
    size_t data_file_element = 0;
    uint8_t data_file_prefix = 0;
    tag_def_s *tag = client->plc->tags;

    info("Got packet:");
    buf_dump(request);

    /* eat the command */
    buf_get_uint8(request);

    offset = buf_get_uint16_le(request);
    transfer_size = buf_get_uint16_le(request);

    /* decode the data file. */
    data_file_prefix = buf_get_uint8(request);

    /* check the data file prefix. */
    if(data_file_prefix != 0x06) {
        info("Unexpected data file prefix byte %d!", data_file_prefix);
        make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
        return PCCC_ERR_ADDR_NOT_USABLE;
    }

    /* get data file number. */
    data_file_num = buf_get_uint8(request);

    /* get the data element number. */
    data_file_element = buf_get_uint8(request);

    /* find the tag. */
    while(tag && tag->data_file_num != data_file_num) {
        tag = tag->next_tag;
    }

    if(!tag) {
        info("Unable to find tag with data file %u!", data_file_num);
        make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
        return PCCC_ERR_ADDR_NOT_USABLE;
    }

    /* now we can check the start and end offsets. */
    tag_size = tag->elem_count * tag->elem_size;
    start_byte_offset = offset + (data_file_element * tag->elem_size);
    end_byte_offset = start_byte_offset + (transfer_size * tag->elem_size);

    if(start_byte_offset >= tag_size) {
        info("Starting offset, %u, is greater than tag size, %d!", (unsigned int)start_byte_offset, (unsigned int)tag_size);
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    if(end_byte_offset > tag_size) {
        info("Ending offset, %u, is greater than tag size, %d!", (unsigned int)end_byte_offset, (unsigned int)tag_size);
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    /* check the amount of data requested. */
    if((end_byte_offset - start_byte_offset) > 240) {
        info("Request asks for too much data, %u bytes, for response packet!", (unsigned int)(end_byte_offset - start_byte_offset));
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    info("Transfer size %u, tag elem size %u, bytes to transfer %d.", transfer_size, tag->elem_size, transfer_size * tag->elem_size);

    /* build the response. */
    buf_set_uint8(response, 0x4f);
    buf_set_uint8(response, 0); /* no error */
    buf_set_uint16_le(response, client->conn_config.pccc_seq_id);

    if(!mutex_lock(&(tag->mutex))) {
        for(size_t i = 0; i < (transfer_size * tag->elem_size); i++) {
            info("setting byte %d to value %d.", 4 + i, tag->data[start_byte_offset + i]);
            buf_set_uint8(response, tag->data[start_byte_offset + i]);
        }

        mutex_unlock(&(tag->mutex));
    } else {
        error("ERROR: Unable to lock mutex!");
    }

    /* cap off response */
    buf_set_len(response, buf_get_cursor(response));

    return rc;
}


int handle_plc5_write_request(tcp_client_p client)
{
    int rc = PCCC_OK;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    uint16_t offset = 0;
    size_t start_byte_offset = 0;
    uint16_t transfer_size = 0;
    size_t end_byte_offset = 0;
    size_t tag_size = 0;
    size_t data_len = 0;
    size_t data_file_num = 0;
    size_t data_file_element = 0;
    uint8_t data_file_prefix = 0;
    tag_def_s *tag = client->plc->tags;

    info("Got request:");
    buf_dump_offset(request, buf_get_cursor(request));

    /* eat the command */
    buf_get_uint8(request);

    offset = buf_get_uint16_le(request);
    transfer_size = buf_get_uint16_le(request);

    /* decode the data file. */
    data_file_prefix = buf_get_uint8(request);

    /* check the data file prefix. */
    if(data_file_prefix != 0x06) {
        info("Unexpected data file prefix byte %d!", data_file_prefix);
        make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
        return PCCC_ERR_ADDR_NOT_USABLE;
    }

    /* get data file number. */
    data_file_num = buf_get_uint8(request);

    /* get the data element number. */
    data_file_element = buf_get_uint8(request);

    /* find the tag. */
    while(tag && tag->data_file_num != data_file_num) {
        tag = tag->next_tag;
    }

    if(!tag) {
        info("Unable to find tag with data file %u!", data_file_num);
        make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
        return PCCC_ERR_ADDR_NOT_USABLE;
    }

    /*
     * we have the tag, now write the data.   The size of the write
     * needs to be less than the tag size.
     */

    tag_size = tag->elem_count * tag->elem_size;
    start_byte_offset = offset + (data_file_element * tag->elem_size);
    end_byte_offset = start_byte_offset + (transfer_size * tag->elem_size);

    if(start_byte_offset >= tag_size) {
        info("Starting offset, %u, is greater than tag size, %d!", (unsigned int)start_byte_offset, (unsigned int)tag_size);
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    if(end_byte_offset > tag_size) {
        info("Ending offset, %u, is greater than tag size, %d!", (unsigned int)end_byte_offset, (unsigned int)tag_size);
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    data_len = buf_len(request) - 8;

    if(data_len != (transfer_size * tag->elem_size)) {
        info("Data in packet is not the same length, %u, as the requested transfer, %d!", data_len, (transfer_size * tag->elem_size));
        make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
        return PCCC_ERR_FILE_IS_WRONG_SIZE;
    }

    /* copy the data into the tag. */
    if(!mutex_lock(&(tag->mutex))) {
        for(size_t i = 0; i < (transfer_size * tag->elem_size); i++) {
            uint8_t tmp_u8 = buf_get_uint8(request);
            info("setting byte %d to value %d.", start_byte_offset + i, tmp_u8);
            tag->data[start_byte_offset + i] = tmp_u8;
        }

        mutex_unlock(&(tag->mutex));
    } else {
        error("ERROR: Unable to lock mutex!");
    }

    info("Transfer size %u, tag elem size %u, bytes to transfer %d.", transfer_size, tag->elem_size, transfer_size * tag->elem_size);

    /* build the response. */
    buf_set_uint8(response, 0x4f);
    buf_set_uint8(response, PCCC_OK); /* no error */
    buf_set_uint16_le(response, client->conn_config.pccc_seq_id);

    /* cap off response */
    buf_set_len(response, buf_get_cursor(response));

    return rc;
}


int handle_slc_read_request(tcp_client_p client)
{
    int rc = PCCC_OK;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    size_t start_byte_offset = 0;
    uint8_t transfer_size = 0;
    size_t end_byte_offset = 0;
    size_t tag_size = 0;
    size_t data_file_num = 0;
    size_t data_file_type = 0;
    size_t data_file_element = 0;
    size_t data_file_subelement = 0;
    tag_def_s *tag = client->plc->tags;

    info("Got request:");
    buf_dump_offset(request, buf_get_cursor(request));

    /*
     * a2 - SLC-type read.
     * <size> - size in bytes to read.
     * <file num> - data file number.
     * <file type> - data file type.
     * <file element> - data file element.
     * <file subelement> - data file subelement.
     */

    /* eat the command */
    buf_get_uint8(request);

    transfer_size = buf_get_uint8(request);
    data_file_num = buf_get_uint8(request);
    data_file_type = buf_get_uint8(request);
    data_file_element = buf_get_uint8(request);
    data_file_subelement = buf_get_uint8(request);

    if(data_file_subelement != 0) {
        info("Data file subelement is unsupported!");
        make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
        return PCCC_ERR_ADDR_NOT_USABLE;
    }

    do {
        /* find the tag. */
        while(tag && tag->data_file_num != data_file_num) {
            tag = tag->next_tag;
        }

        if(!tag) {
            info("Unable to find tag with data file %u!", data_file_num);
            make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
            rc = PCCC_ERR_ADDR_NOT_USABLE;
            break;
        }

        if(tag->tag_type != data_file_type) {
            info("Data file type requested, %u, does not match file type of tag, %d!", data_file_type, tag->tag_type);
            make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
            rc = PCCC_ERR_ADDR_NOT_USABLE;
            break;
        }

        /* now we can check the start and end offsets. */
        tag_size = tag->elem_count * tag->elem_size;
        start_byte_offset = (data_file_element * tag->elem_size);
        end_byte_offset = start_byte_offset + transfer_size;

        info("Start byte offset %u, end byte offset %u.", (unsigned int)start_byte_offset, (unsigned int)end_byte_offset);

        if(start_byte_offset >= tag_size) {
            info("Starting offset, %u, is greater than tag size, %u!", (unsigned int)start_byte_offset, (unsigned int)tag_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        if(end_byte_offset > tag_size) {
            info("Ending offset, %u, is greater than tag size, %u!", (unsigned int)end_byte_offset, (unsigned int)tag_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        /* check the amount of data requested. */
        if(transfer_size > 240) {
            info("Request asks for too much data, %u bytes, for response packet!", (unsigned int)transfer_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        info("Transfer size %u (in bytes), tag elem size %u.", transfer_size, tag->elem_size);

        /* build the response. */
        buf_set_uint8(response, 0x4f);
        buf_set_uint8(response, PCCC_OK); /* no error */
        buf_set_uint16_le(response, client->conn_config.pccc_seq_id);

        if(!mutex_lock(&(tag->mutex))) {
            for(size_t i = 0; i < transfer_size; i++) {
                info("setting byte %d to value %d.", 4 + i, tag->data[start_byte_offset + i]);
                buf_set_uint8(response, tag->data[start_byte_offset + i]);
            }

            mutex_unlock(&(tag->mutex));
        } else {
            error("ERROR: Unable to lock tag mutex!");
        }

        /* cap off the response */
        buf_set_len(response, buf_get_cursor(response));
    } while(0);

    return rc;
}



int handle_slc_write_request(tcp_client_p client)
{
    int rc = PCCC_OK;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    size_t start_byte_offset = 0;
    uint8_t transfer_size = 0;
    size_t end_byte_offset = 0;
    size_t tag_size = 0;
    size_t data_file_num = 0;
    size_t data_file_type = 0;
    size_t data_file_element = 0;
    size_t data_file_subelement = 0;
    size_t data_len = 0;
    tag_def_s *tag = client->plc->tags;

    info("Got request:");
    buf_dump_offset(request, buf_get_cursor(request));

    /*
     * aa - SLC-type write.
     * <size> - size in bytes to write.
     * <file num> - data file number.
     * <file type> - data file type.
     * <file element> - data file element.
     * <file subelement> - data file subelement.
     * ... data ... - data to write.
     */

    /* eat the command */
    buf_get_uint8(request);

    transfer_size = buf_get_uint8(request);
    data_file_num = buf_get_uint8(request);
    data_file_type = buf_get_uint8(request);
    data_file_element = buf_get_uint8(request);
    data_file_subelement = buf_get_uint8(request);

    if(data_file_subelement != 0) {
        info("Data file subelement is unsupported!");
        make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
        return PCCC_ERR_ADDR_NOT_USABLE;
    }

    do {
        /* find the tag. */
        while(tag && tag->data_file_num != data_file_num) {
            tag = tag->next_tag;
        }

        if(!tag) {
            info("Unable to find tag with data file %u!", data_file_num);
            make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
            rc = PCCC_ERR_ADDR_NOT_USABLE;
            break;
        }

        if(tag->tag_type != data_file_type) {
            info("Data file type requested, %x, does not match file type of tag, %x!", data_file_type, tag->tag_type);
            make_pccc_error(client, PCCC_ERR_ADDR_NOT_USABLE);
            rc = PCCC_ERR_ADDR_NOT_USABLE;
            break;
        }

        /* now we can check the start and end offsets. */
        tag_size = tag->elem_count * tag->elem_size;
        start_byte_offset = (data_file_element * tag->elem_size);
        end_byte_offset = start_byte_offset + transfer_size;

        info("Start byte offset %u, end byte offset %u.", (unsigned int)start_byte_offset, (unsigned int)end_byte_offset);

        if(start_byte_offset >= tag_size) {
            info("Starting offset, %u, is greater than tag size, %d!", (unsigned int)start_byte_offset, (unsigned int)tag_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        if(end_byte_offset > tag_size) {
            info("Ending offset, %u, is greater than tag size, %d!", (unsigned int)end_byte_offset, (unsigned int)tag_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        /* check the amount of data requested. */
        if(transfer_size > 240) {
            info("Request asks for too much data, %u bytes, for response packet!", (unsigned int)transfer_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        info("Transfer size %u (in bytes), tag elem size %u.", transfer_size, tag->elem_size);

        data_len = buf_len(request) - buf_get_cursor(request);

        if(data_len != transfer_size) {
            info("Data in packet is not the same length, %u, as the requested transfer, %d!", data_len, transfer_size);
            make_pccc_error(client, PCCC_ERR_FILE_IS_WRONG_SIZE);
            rc = PCCC_ERR_FILE_IS_WRONG_SIZE;
            break;
        }

        /* copy the data into the tag. */
        if(!mutex_lock(&(tag->mutex))) {
            for(size_t i = 0; i < transfer_size; i++) {
                uint8_t tmp_u8 = buf_get_uint8(request);
                info("setting byte %d to value %02x.", start_byte_offset + i, tmp_u8);
                tag->data[start_byte_offset + i] = tmp_u8;
            }

            mutex_unlock(&(tag->mutex));
        } else {
            error("ERROR: Unable to lock mutex!");
        }

        /* build the response. */
        buf_set_uint8(response, 0x4f);
        buf_set_uint8(response, PCCC_OK); /* no error */
        buf_set_uint16_le(response, client->conn_config.pccc_seq_id);

        /* cap off response */
        buf_set_len(response, buf_get_cursor(response));
    } while(0);

    return rc;
}




int make_pccc_error(tcp_client_p client, uint8_t err_code)
{
    buf_t *response = &(client->response);

    // 4f f0 3c 96 06

    buf_set_uint8(response, (uint8_t)0x4f);
    buf_set_uint8(response, (uint8_t)0xf0);
    buf_set_uint16_le(response, client->conn_config.pccc_seq_id);
    buf_set_uint8(response, err_code);

    return PCCC_OK;
}
