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

#include <stdint.h>
#include "cip.h"
#include "cpf.h"
#include "plc.h"
#include "utils/debug.h"
#include "utils/slice.h"
#include "utils/time_utils.h"

#define CPF_ITEM_NAI ((uint16_t)0x0000) /* NULL Address Item */
#define CPF_ITEM_CAI ((uint16_t)0x00A1) /* connected address item */
#define CPF_ITEM_CDI ((uint16_t)0x00B1) /* connected data item */
#define CPF_ITEM_UDI ((uint16_t)0x00B2) /* Unconnected data item */




tcp_connection_status_t cpf_dispatch_connected_request(slice_p request, slice_p response, tcp_connection_p connection_arg)
{
    tcp_connection_status_t conn_rc = TCP_CONNECTION_PDU_STATUS_OK;
    plc_connection_p connection = (plc_connection_p)connection_arg;
    slice_status_t slice_rc;
    uint8_t *saved_start = response->start;
    uint8_t *cip_start = NULL;
    uint16_t payload_size = 0;

    do {
        /* we must have some sort of payload. */
        assert_warn((slice_len(request) >= CPF_CONN_HEADER_SIZE), TCP_CONNECTION_PDU_ERR_INCOMPLETE, "Unusable size of connected CPF packet!");

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

        slice_rc = slice_unpack(request, "u4,u2,u2,u2,u2,u4,u2,^,u2",
                                        &connection->cpf_connection.co_header.interface_handle,
                                        &connection->cpf_connection.co_header.router_timeout,
                                        &connection->cpf_connection.co_header.item_count,
                                        &connection->cpf_connection.co_header.item_addr_type,
                                        &connection->cpf_connection.co_header.item_addr_length,
                                        &connection->cpf_connection.co_header.conn_id,
                                        &connection->cpf_connection.co_header.item_data_type,
                                        &connection->cpf_connection.co_header.item_data_length,
                                        &cip_start,
                                        &connection->cpf_connection.co_header.conn_seq
                            );

        assert_error((slice_rc == SLICE_STATUS_OK), "Error unpacking slice %d!", slice_rc);

        assert_warn((connection->cpf_connection.co_header.item_count == (uint16_t)2), TCP_CONNECTION_PDU_ERR_MALFORMED, "Malformed connected CPF packet, expected two CPF itemas but got %u!", connection->cpf_connection.co_header.item_count);

        assert_warn((connection->cpf_connection.co_header.item_addr_type == CPF_ITEM_CAI), TCP_CONNECTION_PDU_ERR_MALFORMED, "Unsupported connected CPF packet, expected connected address item type but got %u!", connection->cpf_connection.co_header.item_addr_type);

        assert_warn((connection->cpf_connection.co_header.item_addr_size == 0x04), TCP_CONNECTION_PDU_ERR_MALFORMED, "Unsupported connected CPF packet, expected connected address item length of 4 but got %u!", connection->cpf_connection.co_header.item_addr_length);

        assert_warn((connection->cpf_connection.co_header.item_data_type == CPF_ITEM_CDI), TCP_CONNECTION_PDU_ERR_MALFORMED, "Unsupported connected CPF packet, expected connected address item type but got %u!", connection->cpf_connection.co_header.item_addr_type);

        assert_warn((connection->cpf_connection.co_header.conn_id == connection->cip_connection.server_connection_id), TCP_CONNECTION_PDU_ERR_MALFORMED, "Expected connection ID %04x but found connection ID %04x!", connection->cip_connection.server_connection_id, connection->cpf_connection.co_header.conn_id);

        assert_warn((connection->cpf_connection.co_header.item_data_length == (uint16_t)(request->end - cip_start)), TCP_CONNECTION_PDU_ERR_MALFORMED, "CPF payload length, %d, does not match passed length, %d!", (buf_len(request) - cip_start_offset), connection->cpf_connection.co_header.item_data_length);

        /* set up response. */
        saved_start = response->start;
        response->start += CPF_CONN_HEADER_SIZE;

        conn_rc = cip_dispatch_request(request, response, connection_arg);

        payload_size = (uint16_t)(response->end - cip_start);

        assert_warn((conn_rc != TCP_CONNECTION_PDU_STATUS_OK), TCP_CONNECTION_PDU_ERR_MALFORMED, "CIP layer processing failed!");

        /* reset the response start to where it was. */
        response->start = saved_start;

        slice_rc = slice_pack(response, "u4,u2,u2,u2,u2,u4,u2,u2",
                                        connection->cpf_connection.co_header.interface_handle,
                                        connection->cpf_connection.co_header.router_timeout,
                                        connection->cpf_connection.co_header.item_count,
                                        connection->cpf_connection.co_header.item_addr_type,
                                        connection->cpf_connection.co_header.item_addr_length,
                                        connection->cpf_connection.co_header.conn_id,
                                        connection->cpf_connection.co_header.item_data_type,
                                        payload_size,
                                        connection->cpf_connection.co_header.conn_seq
                            );

        assert_error((slice_rc == SLICE_STATUS_OK), "Error packing response slice %d!", slice_rc);

        /* let the previous layer handle the response */
    } while(0);

    /* errors are passed through. */

    return conn_rc;
}


int cpf_dispatch_unconnected_request(slice_p request, slice_p response, tcp_connection_p connection_arg)
{
    tcp_connection_status_t conn_rc = TCP_CONNECTION_PDU_STATUS_OK;
    slice_status_t slice_rc = SLICE_STATUS_OK;
    plc_connection_p connection = (plc_connection_p)connection_arg;
    uint8_t *saved_start = NULL;
    uint16_t payload_size = 0;

    info("got request:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        /* we must have some sort of payload. */
        assert_warn((slice_len(request) >= CPF_UCONN_HEADER_SIZE), TCP_CONNECTION_PDU_ERR_INCOMPLETE, "Unusable size of connected CPF packet!");

        /*
            uint32_t interface_handle;
            uint16_t router_timeout;
            uint16_t item_count;
            uint16_t item_addr_type;
            uint16_t item_addr_length;
            uint16_t item_data_type;
            uint16_t item_data_length;
        */

        slice_rc = slice_unpack(request, "u4,u2,u2,u2,u2,u2,u2",
                                          &connection->cpf_connection.uc_header.interface_handle,
                                          &connection->cpf_connection.uc_header.router_timeout,
                                          &connection->cpf_connection.uc_header.item_count,
                                          &connection->cpf_connection.uc_header.item_addr_type,
                                          &connection->cpf_connection.uc_header.item_addr_length,
                                          &connection->cpf_connection.uc_header.item_data_type,
                                          &connection->cpf_connection.uc_header.item_data_length
                                );

        assert_error((slice_rc == SLICE_STATUS_OK), "Error unpacking slice %d!", slice_rc);

        assert_warn((connection->cpf_connection.uc_header.item_count == (uint16_t)2), TCP_CONNECTION_PDU_ERR_MALFORMED, "Malformed connected CPF packet, expected two CPF itemas but got %u!", connection->cpf_connection.co_header.item_count);

        assert_warn((connection->cpf_connection.uc_header.item_addr_type == CPF_ITEM_NAI), TCP_CONNECTION_PDU_ERR_MALFORMED, "Unsupported connected CPF packet, expected connected address item type but got %u!", connection->cpf_connection.co_header.item_addr_type);

        assert_warn((connection->cpf_connection.uc_header.item_data_type == CPF_ITEM_UDI), TCP_CONNECTION_PDU_ERR_MALFORMED, "Unsupported connected CPF packet, expected connected address item type but got %u!", connection->cpf_connection.co_header.item_addr_type);

        assert_warn((connection->cpf_connection.uc_header.item_data_length == (uint16_t)(request->end - cip_start)), TCP_CONNECTION_PDU_ERR_MALFORMED, "CPF payload length, %d, does not match passed length, %d!", (buf_len(request) - cip_start_offset), connection->cpf_connection.co_header.item_data_length);

        /* set up response. */
        saved_start = response->start;
        response->start += CPF_UCONN_HEADER_SIZE;

        conn_rc = cip_dispatch_request(request, response, connection_arg);

        assert_warn((conn_rc != TCP_CONNECTION_PDU_STATUS_OK), TCP_CONNECTION_PDU_ERR_MALFORMED, "CIP layer processing failed!");

        payload_size = (uint16_t)slice_len(response);

        /* reset the response start to where it was. */
        response->start = saved_start;

        /* generate the CPF response header */
        slice_rc = slice_pack(response, "u4,u2,u2,u2,u2,u2,u2",
                                          connection->cpf_connection.uc_header.interface_handle,
                                          connection->cpf_connection.uc_header.router_timeout,
                                          connection->cpf_connection.uc_header.item_count,
                                          connection->cpf_connection.uc_header.item_addr_type,
                                          connection->cpf_connection.uc_header.item_addr_length,
                                          connection->cpf_connection.uc_header.item_data_type,
                                          payload_size
                            );

        assert_error((slice_rc == SLICE_STATUS_OK), "Error packing response slice %d!", slice_rc);

        /* let the previous layer handle the response */
    } while(0);

    /* errors are pass through. */

    return conn_rc;
}
