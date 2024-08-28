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

    slice_t request_header_slice;
    slice_t request_payload_slice;
    slice_t response_header_slice;
    slice_t response_payload_slice;
} cpf_uc_pdu_t;


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

    slice_t request_header_slice;
    slice_t request_payload_slice;
    slice_t response_header_slice;
    slice_t response_payload_slice;
} cpf_co_pdu_t;


static status_t decode_cpf_co_pdu(slice_p request, cpf_co_pdu_t *pdu);
static status_t decode_cpf_uc_pdu(slice_p request, cpf_uc_pdu_t *pdu);


status_t decode_cpf_co_pdu(slice_p request, cpf_co_pdu_t *pdu)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;

        /* we must have some sort of payload. */
        assert_warn((slice_get_len(request) >= CPF_CONN_HEADER_SIZE), STATUS_BAD_INPUT, "Insufficient data in CPF PDU!");

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

        GET_FIELD(request, u32, &(pdu->interface_handle), sizeof(pdu->interface_handle));
        GET_FIELD(request, u16, &(pdu->router_timeout), sizeof(pdu->router_timeout));
        GET_FIELD(request, u16, &(pdu->item_count), sizeof(pdu->item_count));
        GET_FIELD(request, u16, &(pdu->item_addr_type), sizeof(pdu->item_addr_type));
        GET_FIELD(request, u16, &(pdu->item_addr_length), sizeof(pdu->item_addr_length));
        GET_FIELD(request, u32, &(pdu->conn_id), sizeof(pdu->conn_id));
        GET_FIELD(request, u16, &(pdu->item_data_type), sizeof(pdu->item_data_type));
        GET_FIELD(request, u16, &(pdu->item_data_length), sizeof(pdu->item_data_length));
        GET_FIELD(request, u16, &(pdu->conn_seq), sizeof(pdu->conn_seq));

        /* sanity check the header */
        assert_warn((pdu->item_count == (uint16_t)2), STATUS_BAD_INPUT, "Malformed connected CPF packet, expected two CPF itemas but got %u!", pdu->item_count);

        assert_warn((pdu->item_addr_type == CPF_ITEM_CAI), STATUS_BAD_INPUT, "Unsupported connected CPF packet, expected connected address item type but got %u!", pdu->item_addr_type);

        assert_warn((pdu->item_addr_length == 0x04), STATUS_BAD_INPUT, "Unsupported connected CPF packet, expected connected address item length of 4 but got %u!", pdu->item_addr_length);

        assert_warn((pdu->item_data_type == CPF_ITEM_CDI), STATUS_NOT_RECOGNIZED, "Unsupported connected CPF packet, expected connected address item type but got %u!", pdu->item_addr_type);

        rc = slice_split_at_offset(request, offset, &(pdu->request_header_slice), &(pdu->request_payload_slice));
        if(rc != STATUS_OK) {
            warn("Unable to split request into header and payload! Error: %s", status_to_str(rc));
            break;
        }

        assert_warn((pdu->item_data_length == slice_get_len(&(pdu->request_payload_slice)) + 2), STATUS_BAD_INPUT, "CPF data item length, %d, does not actual payload length, %d!", pdu->item_data_length, slice_get_len(&(pdu->request_payload_slice)) + 2);
    } while(0);

    return rc;
}


status_t cpf_dispatch_connected_request(slice_p request, slice_p response, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    uint8_t *saved_start = response->start;
    uint8_t *cip_start = NULL;
    uint16_t payload_size = 0;
    cpf_co_pdu_t pdu = {0};
    slice_t response_header_slice = {0};
    slice_t response_payload_slice = {0};

    do {
        rc = decode_cpf_co_pdu(request, &pdu);
        if(rc != STATUS_OK) {
            break;
        }

        assert_warn((pdu.conn_id == connection->server_connection_id), STATUS_BAD_INPUT, "Expected server connection ID %08x but found connection ID %08x!", connection->server_connection_id, pdu.conn_id);

        /* store the client connection sequence ID */
        connection->server_connection_seq = pdu.conn_seq;

        /* split up the request to get the payload */
        if(!slice_split_at_offset(request, offset, NULL, &request_payload_slice)) {
            warn("Unable split request slice!");
            break;
        }

        /* split up the response to get the header and payload slices. */
        if(!slice_split_at_offset(response, slice_get_len(&(pdu.request_header_slice)), &(pdu.response_header_slice), &(pdu.response_payload_slice))) {
            warn("Unable split response slice!");
            break;
        }

        rc = cip_process_request(&(pdu.request_payload_slice), &(pdu.response_payload_slice), connection);
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
            rc = STATUS_SETUP_FAILURE;
            break;
        }
    } while(0);

    return rc;
}


status_t cpf_dispatch_unconnected_request(slice_p request, slice_p response, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    cpf_uc_pdu_t header = {0};
    slice_t request_header_slice = {0};
    slice_t request_payload_slice = {0};
    slice_t response_header_slice = {0};
    slice_t response_payload_slice = {0};

    info("got request:");
    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(request), slice_get_end_ptr(request));

    do {
        uint32_t offset = 0;

        /* we must have some sort of payload. */
        assert_warn((slice_get_len(request) >= CPF_UCONN_HEADER_SIZE), STATUS_NO_RESOURCE, "Unusable size of connected CPF packet!");

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
        assert_warn((header.item_count == (uint16_t)2), STATUS_BAD_INPUT, "Malformed connected CPF packet, expected two CPF itemas but got %u!", header.item_count);

        assert_warn((header.item_addr_type == CPF_ITEM_NAI), STATUS_BAD_INPUT, "Unsupported connected CPF packet, expected connected address item type but got %u!", header.item_addr_type);

        assert_warn((header.item_addr_length == (uint16_t)0), STATUS_BAD_INPUT, "Unsupported connected CPF packet, expected connected address item length of 0 but got %u!", header.item_addr_length);

        assert_warn((header.item_data_type == CPF_ITEM_UDI), STATUS_NOT_RECOGNIZED, "Unsupported connected CPF packet, expected connected address item type but got %u!", header.item_addr_type);

        assert_warn((header.item_data_length == slice_get_len(request) - offset), STATUS_BAD_INPUT, "CPF data item length, %d, does not match actual payload length, %d!", header.item_data_length, slice_len(request) - offset);

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

        rc = cip_process_request(&request_payload_slice, &response_payload_slice, connection);
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
            rc = STATUS_SETUP_FAILURE;
            break;
        }
    } while(0);

    return rc;
}
