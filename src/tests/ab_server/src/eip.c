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
#include <stdlib.h>
#include <string.h>
#include "cpf.h"
#include "eip.h"
#include "utils/debug.h"
#include "utils/slice.h"
#include "utils/tcp_server.h"
#include "utils/time_utils.h"

#define EIP_REGISTER_SESSION     ((uint16_t)0x0065)
#define EIP_UNREGISTER_SESSION   ((uint16_t)0x0066)
#define EIP_UNCONNECTED_SEND     ((uint16_t)0x006F)
#define EIP_CONNECTED_SEND       ((uint16_t)0x0070)

#define EIP_REGISTER_SESSION_SIZE (4) /* 4 bytes, 2 16-bit words */


/* supported EIP version */
#define EIP_VERSION     ((uint16_t)1)

static tcp_connection_status_t decode_eip_header(slice_p request, eip_header_p header, slice_p header_slice, slice_p payload_slice);
static tcp_connection_status_t encode_eip_header(slice_p response, eip_header_p header);
static tcp_connection_status_t reserve_eip_header(slice_p response, eip_header_p header);




static tcp_connection_status_t register_session(slice_p request, slice_p parent_response, plc_connection_p connection_arg);
static tcp_connection_status_t unregister_session(slice_p request, slice_p parent_response, plc_connection_p connection_arg);



tcp_connection_status_t eip_dispatch_request(slice_p request, slice_p response, tcp_connection_p connection_arg)
{
    tcp_connection_status_t rc = TCP_CONNECTION_PDU_STATUS_OK;
    slice_status_t slice_rc = SLICE_STATUS_OK;
    plc_connection_p connection = (plc_connection_p)connection_arg;
    uint8_t *saved_start = NULL;
    slice_t response_header_slice = {0};
    slice_t response_payload_slice = {0};
    eip_header_t eip_header = {0};

    info("eip_dispatch_request(): got packet:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        uint32_t session_handle = 0;

        assert_detail((slice_len(request) >= EIP_HEADER_SIZE), TCP_CONNECTION_PDU_INCOMPLETE, "Not enough data for the EIP header.  Go get more.");

        rc = decode_eip_header(request, &eip_header, &response_header_slice, &response_payload_slice);

        if(rc != TCP_CONNECTION_OK) {
            warn("Unable to decode the EIP header!");
            break;
        }

        /* push the response end out to the full buffer. */
        response_payload_slice.end = response->end;

        /* sanity checks */
        assert_detail((slice_len(request) > (uint32_t)(header.length + (uint32_t)EIP_HEADER_SIZE)),
                       TCP_CONNECTION_PDU_INCOMPLETE,"Partial EIP packet, returning for more data."
                     );

        /* If we have a session handle already, we must match it with the incoming request */
        assert_warn(((!connection->eip_connection.eip_header.session_handle) || (header.session_handle == connection->eip_connection.eip_header.session_handle)),
                     TCP_CONNECTION_PDU_ERR_MALFORMED,
                     "Request session handle %08"PRIx32" does not match the one for this connection, %08"PRIx32"!",
                     session_handle,
                     connection->eip_connection.eip_header.session_handle
                    );

        /* dispatch the request */
        switch(connection->eip_connection.eip_header.command) {
            case EIP_REGISTER_SESSION:
                rc = register_session(request, &response_payload_slice, connection);
                break;

            case EIP_UNREGISTER_SESSION:
                rc = unregister_session(request, &response_payload_slice, connection);
                break;

            case EIP_CONNECTED_SEND:
                rc = cpf_dispatch_connected_request(request, &response_payload_slice, connection);
                break;

            case EIP_UNCONNECTED_SEND:
                rc = cpf_dispatch_unconnected_request(request, &response_payload_slice, connection);
                break;

            default:
                rc = PDU_ERR_NOT_RECOGNIZED;
                break;
        }

        /* are we terminating the connection or so broken that we need to? */
        if(rc == TCP_CONNECTION_CLOSE) {
            break;
        }

        if(rc == TCP_CONNECTION_PDU_STATUS_OK) {
            uint32_t payload_length = slice_len(response);

            /* set up the response. */
            response->start = saved_start;

            slice_rc = slice_pack(response, "u2,u2,u4,u4,u8,u4",
                                            connection->eip_connection.command,
                                            (uint16_t)(payload_length),
                                            connection->eip_connection.session_handle,
                                            connection->eip_connection.status,
                                            connection->eip_connection.sender_context,
                                            connection->eip_connection.options
                                 );

            if(slice_rc == SLICE_ERR_TOO_LITTLE_SPACE) {
                warn("Insufficient space in the parent_response buffer!");
                break;
            }

            assert_warn((slice_rc != SLICE_ERR_TOO_LITTLE_SPACE), TCP_CONNECTION_PDU_INCOMPLETE, "Insufficient space in the parent_response buffer!");

            assert_error((slice_rc == SLICE_STATUS_OK), "Fatal slice error, %d, processing data!", slice_rc);
        }
    } while(0);

    if(rc == TCP_CONNECTION_PDU_STATUS_OK) {
        /* if there is a parent_response delay requested, then wait a bit. */
        if(connection->eip_connection.response_delay > 0) {
            util_sleep_ms(connection->eip_connection.response_delay);
        }
    }

    return rc;
}



tcp_connection_status_t register_session(slice_p request, slice_p response, plc_connection_p connection)
{
    tcp_connection_status_t rc = TCP_CONNECTION_PDU_STATUS_OK;
    slice_status_t slice_rc = SLICE_STATUS_OK;
    uint8_t *saved_start = response->start;

    struct {
        uint16_t eip_version;
        uint16_t option_flags;
    } register_request;

    do {
        slice_rc = slice_unpack(request, "u2,u2",
                                          &register_request.eip_version,
                                          &register_request.option_flags
                               );

        assert_error((slice_rc == SLICE_STATUS_OK), "Error with unpacking from the request slice!");

        /* sanity checks.  The command and packet length are checked by now. */

        /* session_handle must be zero. */
        assert_warn((connection->eip_connection.session_handle == (uint32_t)0), TCP_CONNECTION_PDU_ERR_MALFORMED, "Request failed sanity check: request session handle is %04"PRIx32" but should be zero.", connection->eip_connection.session_handle);

        /* status must be zero. */
        assert_warn((connection->eip_connection.status == (uint32_t)0), TCP_CONNECTION_PDU_ERR_MALFORMED, "Request failed sanity check: request status is %04"PRIx32" but should be zero.", connection->eip_connection.status);

        /* sender context must be zero? */
        assert_warn((connection->eip_connection.sender_context == (uint64_t)0), TCP_CONNECTION_PDU_ERR_MALFORMED, "Request failed sanity check: request sender context is %08"PRIx64" but should be zero.", connection->eip_connection.sender_context);

        /* request EIP version must be 1 (one). */
        assert_warn((register_request.eip_version == (uint32_t)1), TCP_CONNECTION_PDU_ERR_MALFORMED, "Request failed sanity check: request EIP version is %04"PRIx32" but should be one (1).", register_request.eip_version);

        /* request option flags must be zero. */
        assert_warn((register_request.option_flags == (uint32_t)0), TCP_CONNECTION_PDU_ERR_MALFORMED, "Request failed sanity check: request option flags is %04"PRIx32" but should be zero.", register_request.option_flags);

        /* all good, generate a session handle. */
        connection->eip_connection.session_handle = (uint32_t)rand();

        slice_rc = slice_pack(response, "u2,u2",
                                         &register_request.eip_version,
                                         &register_request.option_flags
                             );

        assert_error((slice_rc == SLICE_STATUS_OK), "Error with packing into the parent_response slice!");

        /* cap off the response. */
        response->end = response->start;
        response->start = saved_start;
    } while(0);

    return rc;
}


tcp_connection_status_t unregister_session(slice_p request, slice_p parent_response, tcp_connection_p connection)
{
    (void)request;
    (void)parent_response;
    (void)connection;

    /* shut down the connection */
    return TCP_CONNECTION_CLOSE;
}


tcp_connection_status_t decode_eip_header(slice_p request, eip_header_p header, slice_p header_slice, slice_p payload_slice)
{
    tcp_connection_status_t rc = TCP_CONNECTION_PDU_STATUS_OK;
    slice_status_t slice_rc = SLICE_STATUS_OK;
    slice_t payload = {0};

    do {
        assert_warn((header), PDU_ERR_INTERNAL, "Header pointer is null!");

        assert_detail((slice_len(request) >= EIP_HEADER_SIZE), TCP_CONNECTION_PDU_INCOMPLETE, "Not enough data for the EIP header.");

        memset(header, 0, sizeof(*header));

        /* unpack EIP header. */
        slice_rc = slice_unpack(request, "u2,u2,u4,u4,u8,u4,|",
                                &header->command,
                                &header->length,
                                &header->session_handle,
                                &header->status,
                                &header->sender_context,
                                &header->options,
                                header_slice,
                                payload_slice
                            );

        assert_info((slice_rc != SLICE_ERR_TOO_LITTLE_DATA), PDU_ERR_INCOMPLETE, "Not enough data to unpack EIP header.");

        assert_warn((slice_rc == SLICE_STATUS_OK), PDU_ERR_INTERNAL, "Unable to unpack EIP header!");
    } while(0);

    return rc;
}


pdu_status_t encode_eip_header(slice_p response, eip_header_p header)
{
    tcp_connection_status_t rc = PDU_STATUS_OK;
    slice_status_t slice_rc = SLICE_STATUS_OK;
    uint8_t *saved_start = NULL;

    do {
        assert_warn((header), PDU_ERR_INTERNAL, "Header pointer is null!");

        assert_detail((slice_len(response) >= EIP_HEADER_SIZE), PDU_ERR_NO_SPACE, "Not enough data for the EIP header.");

        saved_start = response->start;

        /* unpack EIP header. */
        slice_rc = slice_pack(response, "u2,u2,u4,u4,u8,u4",
                                header->command,
                                (uint16_t)(slice_len(&(header->payload))),
                                header->session_handle,
                                header->status,
                                header->sender_context,
                                header->options
                            );

        assert_info((slice_rc != SLICE_ERR_TOO_LITTLE_DATA), PDU_ERR_INCOMPLETE, "Not enough data to unpack EIP header.");

        assert_warn((slice_rc == SLICE_STATUS_OK), PDU_ERR_INTERNAL, "Unable to unpack EIP header!");

        response->start = saved_start;
    } while(0);

    return rc;
}


pdu_status_t reserve_eip_header(slice_p response)
{
    pdu_status_t rc = PDU_STATUS_OK;

    do {
        assert_warn((slice_len(response) >= EIP_HEADER_SIZE), PDU_ERR_NO_SPACE, "Insuffienct space in response buffer for EIP header!");

        response->start += EIP_HEADER_SIZE;
    } while(0);

    return rc;
}
