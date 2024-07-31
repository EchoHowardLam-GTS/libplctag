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

#include <stdint.h>
#include "cip.h"
#include "cpf.h"
#include "eip.h"
#include "utils.h"

#define CPF_ITEM_NAI ((uint16_t)0x0000) /* NULL Address Item */
#define CPF_ITEM_CAI ((uint16_t)0x00A1) /* connected address item */
#define CPF_ITEM_CDI ((uint16_t)0x00B1) /* connected data item */
#define CPF_ITEM_UDI ((uint16_t)0x00B2) /* Unconnected data item */


typedef struct {
    uint32_t interface_handle;
    uint16_t router_timeout;
    uint16_t item_count;        /* should be 2 for now. */
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint16_t item_data_type;
    uint16_t item_data_length;
} cpf_uc_header_s;

#define CPF_UCONN_HEADER_SIZE (16)

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
} cpf_co_header_s;

#define CPF_CONN_HEADER_SIZE (22)




int handle_cpf_connected(tcp_client_p client)
{
    int rc;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    uint16_t cpf_start_offset = 0;
    uint16_t cip_start_offset = 0;
    cpf_co_header_s header;

    /* we must have some sort of payload. */
    if(buf_len(request) <= CPF_CONN_HEADER_SIZE) {
        info("Unusable size of connected CPF packet!");
        return EIP_ERR_BAD_REQUEST;
    }

    cpf_start_offset = buf_get_start(request);

    /* unpack the request. */
    header.interface_handle = buf_get_uint32_le(request);
    header.router_timeout = buf_get_uint16_le(request);
    header.item_count = buf_get_uint16_le(request);

    /* sanity check the number of items. */
    if(header.item_count != (uint16_t)2) {
        info("Unsupported connected CPF packet, expected two items but found %u!", header.item_count);
        return EIP_ERR_BAD_REQUEST;
    }

    header.item_addr_type = buf_get_uint16_le(request);
    header.item_addr_length = buf_get_uint16_le(request);
    header.conn_id = buf_get_uint32_le(request);
    header.item_data_type = buf_get_uint16_le(request);
    header.item_data_length = buf_get_uint16_le(request);

    /* for some reason the connection sequence ID is
     * considered to be part of the CIP packet not the CPF
     * packet.   So get the cursor here and then get the seq ID.
     */
    cip_start_offset = buf_start(request) + buf_get_cursor(request);

    header.conn_seq = buf_get_uint16_le(request);

    /* sanity check the data. */
    if(header.item_addr_type != CPF_ITEM_CAI) {
        info("Expected connected address item but found %x!", header.item_addr_type);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.item_addr_length != 4) {
        info("Expected address item length of 4 but found %d bytes!", header.item_addr_length);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.conn_id != client->conn_config.server_connection_id) {
        info("Expected connection ID %x but found connection ID %x!", client->conn_config.server_connection_id, header.conn_id);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.item_data_type != CPF_ITEM_CDI) {
        info("Expected connected data item but found %x!", header.item_data_type);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.item_data_length != (buf_len(request) - cip_start_offset)) {
        info("CPF payload length, %d, does not match passed length, %d!", (buf_len(request) - cip_start_offset), header.item_data_length);
        return EIP_ERR_BAD_REQUEST;
    }

    /* do we care about the sequence ID?   Should check. */
    client->conn_config.server_connection_seq = header.conn_seq;

    /*
     * note that we take the cursor from the request and apply it to the response.
     * This is OK because everything before the CIP packet is fixed length.  Probably a
     * bit shady though.
     */
    buf_set_start(request, buf_get_cursor(request));
    buf_set_cursor(request, 0);

    buf_set_start(response, buf_get_cursor(request));
    buf_set_cursor(request, 0);

    rc = cip_dispatch_request(client);

    if(rc == CIP_OK) {
        /* build outbound header. */
        uint16_t cip_payload_size = buf_len(response);

        buf_set_start(response, cpf_start_offset);
        buf_set_cursor(response, 0);

        buf_set_uint32_le(response, header.interface_handle);
        buf_set_uint16_le(response, header.router_timeout);
        buf_set_uint16_le(response, 2); /* two items. */
        buf_set_uint16_le(response, CPF_ITEM_CAI); /* connected address type. */
        buf_set_uint16_le(response, 4); /* connection ID is 4 bytes. */
        buf_set_uint32_le(response, client->conn_config.client_connection_id);
        buf_set_uint16_le(response, CPF_ITEM_CDI); /* connected data type */
        buf_set_uint16_le(response, (uint16_t)(cip_payload_size + 2)); /* MAGIC, +2 for the connection sequence ID */
        buf_set_uint16_le(response, client->conn_config.server_connection_seq);
    }

    /* errors are pass through. */

    return rc;
}


int handle_cpf_unconnected(tcp_client_p client)
{
    int rc;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    uint16_t cpf_start_offset = 0;
    uint16_t cip_start_offset = 0;
    cpf_uc_header_s header;

    info("handle_cpf_unconnected(): got request:");
    buf_dump(request);

    /* we must have some sort of payload. */
    if(buf_len(request) <= CPF_UCONN_HEADER_SIZE) {
        info("Unusable size of unconnected CPF packet!");
        return EIP_ERR_BAD_REQUEST;
    }

    cpf_start_offset = buf_get_start(request);

    /* unpack the request. The caller set the cursor. */
    header.interface_handle = buf_get_uint32_le(request);
    header.router_timeout = buf_get_uint16_le(request);
    header.item_count = buf_get_uint16_le(request);

    /* sanity check the number of items. */
    if(header.item_count != (uint16_t)2) {
        info("Unsupported unconnected CPF packet, expected two items but found %u!", header.item_count);
        return EIP_ERR_BAD_REQUEST;
    }

    header.item_addr_type = buf_get_uint16_le(request);
    header.item_addr_length = buf_get_uint16_le(request);
    header.item_data_type = buf_get_uint16_le(request);
    header.item_data_length = buf_get_uint16_le(request);

    /* sanity check the data. */
    if(header.item_addr_type != CPF_ITEM_NAI) {
        info("Expected null address item but found %x!", header.item_addr_type);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.item_addr_length != 0) {
        info("Expected zero address item length but found %d bytes!", header.item_addr_length);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.item_data_type != CPF_ITEM_UDI) {
        info("Expected unconnected data item but found %x!", header.item_data_type);
        return EIP_ERR_BAD_REQUEST;
    }

    if(header.item_data_length != (buf_len(request) - buf_get_cursor(request))) {
        info("CPF unconnected payload length, %d, does not match passed length, %d!", (buf_len(request) - buf_get_cursor(request)), header.item_data_length);
        return EIP_ERR_BAD_REQUEST;
    }

    /* dispatch and handle the result. Set the cursor in the right place in the response. */

    /*
     * note that we take the cursor from the request and apply it to the response.
     * This is OK because everything before the CIP packet is fixed length.  Probably a
     * bit shady though.
     */
    cip_start_offset = buf_get_cursor(request);

    buf_set_start(request, cip_start_offset);
    buf_set_cursor(request, 0);

    buf_set_start(response, cip_start_offset);
    buf_set_cursor(response, 0);

    rc = cip_dispatch_request(client);

    if(rc == CIP_OK) {
        /* build outbound header. */
        uint16_t cip_payload_size = buf_len(response);

        buf_set_start(response, cpf_start_offset);
        buf_set_cursor(response, 0);

        buf_set_uint32_le(response, header.interface_handle);
        buf_set_uint16_le(response, header.router_timeout);
        buf_set_uint16_le(response, 2); /* two items. */
        buf_set_uint16_le(response, CPF_ITEM_NAI); /* connected address type. */
        buf_set_uint16_le(response, 0); /* No connection ID. */
        buf_set_uint16_le(response, CPF_ITEM_UDI); /* connected data type */
        buf_set_uint16_le(response, (uint16_t)(cip_payload_size));
    }

    /* errors are pass through. */

    return rc;
}
