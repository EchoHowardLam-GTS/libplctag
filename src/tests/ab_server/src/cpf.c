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
#include <stdint.h>
#include "cip.h"
#include "cpf.h"
#include "plc.h"
#include "utils/debug.h"
#include "utils/slice.h"
#include "utils/status.h"
#include "utils/time_utils.h"

const uint16_t CPF_ITEM_NAI = ((uint16_t)0x0000); /* NULL Address Item */
const uint16_t CPF_ITEM_CAI = ((uint16_t)0x00A1); /* connected address item */
const uint16_t CPF_ITEM_CDI = ((uint16_t)0x00B1); /* connected data item */
const uint16_t CPF_ITEM_UDI = ((uint16_t)0x00B2); /* Unconnected data item */


const uint32_t CPF_CONN_HEADER_SIZE = 22;
const uint32_t CPF_UCONN_HEADER_SIZE = 16;


typedef struct {
    uint32_t interface_handle;
    uint16_t router_timeout;
    uint16_t item_count;
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint16_t item_data_type;
    uint16_t item_data_length;
} cpf_uc_header_t;


typedef struct {
    uint32_t interface_handle;
    uint16_t router_timeout;
    uint16_t item_count;        /* should be 2 for now. */
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint32_t conn_id;
    uint16_t item_data_type;
    uint16_t item_data_length;
    uint16_t conn_seq;
} cpf_co_header_t;



status_t cpf_dispatch_connected_request(slice_p request, slice_p response, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    uint8_t *saved_start = response->start;
    uint8_t *cip_start = NULL;
    uint16_t payload_size = 0;
    cpf_co_header_t header = {0};
    slice_t request_payload_slice = {0};
    slice_t response_header_slice = {0};
    slice_t response_payload_slice = {0};

    do {
        uint32_t offset = 0;

        /* we must have some sort of payload. */
        assert_warn((slice_get_len(request) >= CPF_CONN_HEADER_SIZE), STATUS_ERR_RESOURCE, "Insufficient data in CPF PDU!");

        /*
            uint32_t interface_handle;
            uint16_t router_timeout;
            uint16_t item_count;
            uint16_t item_addr_type;
            uint16_t item_addr_length;
            uint32_t conn_id;
            uint16_t item_data_type;
            uint16_t item_data_length;
            uint16_t conn_seq;
        */

        GET_FIELD(request, u32, &(header.interface_handle), sizeof(header.interface_handle));
        GET_FIELD(request, u16, &(header.router_timeout), sizeof(header.router_timeout));
        GET_FIELD(request, u16, &(header.item_count), sizeof(header.item_count));
        GET_FIELD(request, u16, &(header.item_addr_type), sizeof(header.item_addr_type));
        GET_FIELD(request, u16, &(header.item_addr_length), sizeof(header.item_addr_length));
        GET_FIELD(request, u32, &(header.conn_id), sizeof(header.conn_id));
        GET_FIELD(request, u16, &(header.item_data_type), sizeof(header.item_data_type));
        GET_FIELD(request, u16, &(header.item_data_length), sizeof(header.item_data_length));
        GET_FIELD(request, u16, &(header.conn_seq), sizeof(header.conn_seq));

        /* sanity check the header */
        assert_warn((header.item_count == (uint16_t)2), STATUS_ERR_PARAM, "Malformed connected CPF packet, expected two CPF itemas but got %u!", header.item_count);

        assert_warn((header.item_addr_type == CPF_ITEM_CAI), STATUS_ERR_PARAM, "Unsupported connected CPF packet, expected connected address item type but got %u!", header.item_addr_type);

        assert_warn((header.item_addr_length == 0x04), STATUS_ERR_PARAM, "Unsupported connected CPF packet, expected connected address item length of 4 but got %u!", header.item_addr_length);

        assert_warn((header.item_data_type == CPF_ITEM_CDI), STATUS_ERR_NOT_RECOGNIZED, "Unsupported connected CPF packet, expected connected address item type but got %u!", header.item_addr_type);

        assert_warn((header.conn_id == connection->server_connection_id), STATUS_ERR_PARAM, "Expected server connection ID %08x but found connection ID %08x!", connection->server_connection_id, header.conn_id);

        assert_warn((header.item_data_length == slice_len(request) - offset + 2), STATUS_ERR_PARAM, "CPF data item length, %d, does not actual payload length, %d!", header.item_data_length, slice_len(request) - offset + 2);

        /* store the client connection sequence ID */
        connection->server_connection_seq = header.conn_seq;

        /* split up the request to get the payload */
        if(!slice_split_at_offset(request, offset, NULL, &request_payload_slice)) {
            warn("Unable split request slice!");
            break;
        }

        /* split up the response to get the header and payload slices. */
        if(!slice_split_at_offset(response, offset, &response_header_slice, &response_payload_slice)) {
            warn("Unable split response slice!");
            break;
        }

        rc = cip_dispatch_request(&request_payload_slice, &response_payload_slice, connection);
        if(rc != STATUS_OK) {
            warn("Unable to dispatch CIP request!");
            break;
        }

        /* build up the response.  */
        connection->client_connection_seq++;
        offset = 0;
        SET_FIELD(&response_header_slice, u32, header.interface_handle, sizeof(header.interface_handle));
        SET_FIELD(&response_header_slice, u16, header.router_timeout, sizeof(header.router_timeout));
        SET_FIELD(&response_header_slice, u16, header.item_count, sizeof(header.item_count));
        SET_FIELD(&response_header_slice, u16, header.item_addr_type, sizeof(header.item_addr_type));
        SET_FIELD(&response_header_slice, u16, header.item_addr_length, sizeof(header.item_addr_length));
        SET_FIELD(&response_header_slice, u32, connection->client_connection_id, sizeof(header.conn_id));
        SET_FIELD(&response_header_slice, u16, header.item_data_type, sizeof(header.item_data_type));
        SET_FIELD(&response_header_slice, u16, (uint16_t)(slice_len(&response_payload_slice) + 2), sizeof(header.item_data_length));
        SET_FIELD(&response_header_slice, u16, connection->client_connection_seq, sizeof(header.conn_seq));

        detail("CPF response has a payload of %"PRIu32" bytes.", slice_get_len(&response_payload_slice));

        /* truncate the response to the header plus payload */
        if(!slice_truncate_to_offset(response, slice_get_len(&response_header_slice) + slice_get_len(&response_payload_slice))) {
            warn("Unable to truncate response slice!");
            rc = STATUS_ERR_OP_FAILED;
            break;
        }
    } while(0);

    return rc;
}


int cpf_dispatch_unconnected_request(slice_p request, slice_p response, tcp_connection_p connection_arg)
{
    status_t rc = STATUS_OK;
    plc_connection_p connection = (plc_connection_p)connection_arg;
    cpf_uc_header_t header = {0};
    slice_t request_header_slice = {0};
    slice_t request_payload_slice = {0};
    slice_t response_header_slice = {0};
    slice_t response_payload_slice = {0};

    info("got request:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        uint32_t offset = 0;

        /* we must have some sort of payload. */
        assert_warn((slice_get_len(request) >= CPF_UCONN_HEADER_SIZE), STATUS_ERR_RESOURCE, "Unusable size of connected CPF packet!");

        /*
            uint32_t interface_handle;
            uint16_t router_timeout;
            uint16_t item_count;
            uint16_t item_addr_type;
            uint16_t item_addr_length;
            uint16_t item_data_type;
            uint16_t item_data_length;
        */

        GET_FIELD(request, u32, &(header.interface_handle), sizeof(header.interface_handle));
        GET_FIELD(request, u16, &(header.router_timeout), sizeof(header.router_timeout));
        GET_FIELD(request, u16, &(header.item_count), sizeof(header.item_count));
        GET_FIELD(request, u16, &(header.item_addr_type), sizeof(header.item_addr_type));
        GET_FIELD(request, u16, &(header.item_addr_length), sizeof(header.item_addr_length));
        GET_FIELD(request, u16, &(header.item_data_type), sizeof(header.item_data_type));
        GET_FIELD(request, u16, &(header.item_data_length), sizeof(header.item_data_length));

        /* sanity check the header */
        assert_warn((header.item_count == (uint16_t)2), STATUS_ERR_PARAM, "Malformed connected CPF packet, expected two CPF itemas but got %u!", header.item_count);

        assert_warn((header.item_addr_type == CPF_ITEM_NAI), STATUS_ERR_PARAM, "Unsupported connected CPF packet, expected connected address item type but got %u!", header.item_addr_type);

        assert_warn((header.item_addr_length == (uint16_t)0), STATUS_ERR_PARAM, "Unsupported connected CPF packet, expected connected address item length of 0 but got %u!", header.item_addr_length);

        assert_warn((header.item_data_type == CPF_ITEM_UDI), STATUS_ERR_NOT_RECOGNIZED, "Unsupported connected CPF packet, expected connected address item type but got %u!", header.item_addr_type);

        assert_warn((header.item_data_length == slice_len(request) - offset), STATUS_ERR_PARAM, "CPF data item length, %d, does not match actual payload length, %d!", header.item_data_length, slice_len(request) - offset);

        /* split up the request to get the payload */
        if(!slice_split_at_offset(request, offset, NULL, &request_payload_slice)) {
            warn("Unable split request slice!");
            break;
        }

        /* split up the response to get the header and payload slices. */
        if(!slice_split_at_offset(response, offset, &response_header_slice, &response_payload_slice)) {
            warn("Unable split response slice!");
            break;
        }

        rc = cip_dispatch_request(&request_payload_slice, &response_payload_slice, connection);
        if(rc != STATUS_OK) {
            warn("Unable to dispatch CIP request!");
            break;
        }

        /* build up the response.  */
        offset = 0;
        SET_FIELD(&response_header_slice, u32, header.interface_handle, sizeof(header.interface_handle));
        SET_FIELD(&response_header_slice, u16, header.router_timeout, sizeof(header.router_timeout));
        SET_FIELD(&response_header_slice, u16, header.item_count, sizeof(header.item_count));
        SET_FIELD(&response_header_slice, u16, header.item_addr_type, sizeof(header.item_addr_type));
        SET_FIELD(&response_header_slice, u16, header.item_addr_length, sizeof(header.item_addr_length));
        SET_FIELD(&response_header_slice, u16, header.item_data_type, sizeof(header.item_data_type));
        SET_FIELD(&response_header_slice, u16, (uint16_t)slice_len(&response_payload_slice), sizeof(header.item_data_length));

        detail("CPF response has a payload of %"PRIu32" bytes.", slice_get_len(&response_payload_slice));

        /* truncate the response to the header plus payload */
        if(!slice_truncate_to_offset(response, slice_get_len(&response_header_slice) + slice_get_len(&response_payload_slice))) {
            warn("Unable to truncate response slice!");
            rc = STATUS_ERR_OP_FAILED;
            break;
        }
    } while(0);

    return rc;
}
