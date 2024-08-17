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
#include "plc.h"
#include "utils/debug.h"
#include "utils/slice.h"
#include "utils/status.h"
#include "utils/tcp_server.h"
#include "utils/time_utils.h"

typedef uint16_t eip_command_t;

const eip_command_t EIP_REGISTER_SESSION     = ((eip_command_t)0x0065);
const eip_command_t EIP_UNREGISTER_SESSION   = ((eip_command_t)0x0066);
const eip_command_t EIP_UNCONNECTED_SEND     = ((eip_command_t)0x006F);
const eip_command_t EIP_CONNECTED_SEND       = ((eip_command_t)0x0070);

const uint16_t EIP_REGISTER_SESSION_SIZE  = ((size_t)4); /* 4 bytes, 2 16-bit words */


/* supported EIP version */
const uint16_t EIP_VERSION     = ((uint16_t)1);

static status_t decode_eip_pdu(slice_p request, eip_pdu_header_p eip_pdu_header, slice_p request_header_slice, slice_p request_payload_slice);
static status_t encode_eip_pdu(slice_p pdu_header_slice, eip_pdu_header_p eip_pdu_header, slice_p pdu_payload_slice);
static status_t reserve_eip_pdu(slice_p response, slice_p pdu_header_slice, slice_p eip_pdu_header_p);




static status_t register_session(slice_p request, slice_p response, plc_connection_p connection);
static status_t unregister_session(slice_p request, slice_p response, plc_connection_p connection);



// #define GET_FIELD(SLICE, TYPE, ADDR, SIZE)                                           \
//         if(slice_get_ ## TYPE ## _le_at_offset((SLICE), offset, (ADDR))) {    \
//             warn("Unable to get field at offset %"PRIu32"!", offset);         \
//             rc = STATUS_ERR_OP_FAILED;                                            \
//             break;                                                            \
//         }                                                                     \
//         offset += (SIZE)


// #define SET_FIELD(SLICE, TYPE, VAL, SIZE)                                                  \
//         if(slice_set_ ## TYPE ## _le_at_offset((SLICE), offset, (VAL))) {  \
//             warn("Unable to set field at offset %"PRIu32"!", offset);               \
//             rc = STATUS_ERR_OP_FAILED;                                                  \
//             break;                                                                  \
//         }                                                                           \
//         offset += (SIZE)




status_t eip_dispatch_request(slice_p request, slice_p response, tcp_connection_p connection_arg)
{
    status_t rc = STATUS_OK;
    status_t slice_rc = STATUS_OK;
    plc_connection_p connection = (plc_connection_p)connection_arg;
    uint8_t *saved_start = NULL;
    slice_t request_header_slice = {0};
    slice_t request_payload_slice = {0};
    slice_t response_header_slice = {0};
    slice_t response_payload_slice = {0};
    eip_pdu_header_t eip_pdu_header = {0};

    info("eip_dispatch_request(): got packet:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        uint32_t session_handle = 0;

        assert_detail((slice_get_len(request) >= EIP_HEADER_SIZE), STATUS_ERR_RESOURCE, "Not enough data for the EIP request PDU.");

        slice_rc = decode_eip_pdu(request, &eip_pdu_header, &request_header_slice, &request_payload_slice);
        if(slice_rc != STATUS_OK) {
            warn("Unable to decode the EIP eip_pdu_header!");
            rc = STATUS_ERR_OP_FAILED;
            break;
        }

        /* sanity checks */
        assert_info((slice_get_len(request) >= (uint32_t)(eip_pdu_header.length + (uint32_t)EIP_HEADER_SIZE)),
                       STATUS_ERR_RESOURCE,
                       "Partial EIP packet, returning for more data."
                     );

        /* If we have a session handle already, we must match it with the incoming request */
        assert_warn(((!connection->session_handle) || (eip_pdu_header.session_handle == connection->session_handle)),
                     STATUS_ERR_PARAM,
                     "Request session handle %08"PRIx32" does not match the one for this connection, %08"PRIx32"!",
                     eip_pdu_header.session_handle,
                     connection->session_handle
                    );

        /* set up the response slices */
        slice_rc = reserve_eip_pdu(response, &response_header_slice, &response_payload_slice);
        if(slice_rc != STATUS_OK) {
            warn("Unable to reserve response header and payload slices!");
            rc = STATUS_ERR_OP_FAILED;
            break;
        }

        /* dispatch the request */
        switch(eip_pdu_header.command) {
            case EIP_REGISTER_SESSION:
                rc = register_session(&request_payload_slice, &response_payload_slice, connection);
                break;

            case EIP_UNREGISTER_SESSION:
                rc = unregister_session(&request_payload_slice, &response_payload_slice, connection);
                break;

            case EIP_CONNECTED_SEND:
                rc = cpf_dispatch_connected_request(&request_payload_slice, &response_payload_slice, connection);
                break;

            case EIP_UNCONNECTED_SEND:
                rc = cpf_dispatch_unconnected_request(&request_payload_slice, &response_payload_slice, connection);
                break;

            default:
                rc = STATUS_ERR_NOT_RECOGNIZED;
                break;
        }

        /* are we terminating the connection or so broken that we need to? */
        if(rc == STATUS_TERMINATE) {
            break;
        }

        /* build up the response if everything was good. */
        if(rc == STATUS_OK) {
            detail("EIP response has a payload of %"PRIu32" bytes.", slice_get_len(&response_payload_slice));

            /* truncate the response to the header plus payload */
            if(!slice_truncate_to_offset(response, slice_get_len(&response_header_slice) + slice_get_len(&response_payload_slice))) {
                warn("Unable to truncate response slice!");
                rc = STATUS_ERR_OP_FAILED;
                break;
            }

            /* set the session handle */
            eip_pdu_header.session_handle = connection->session_handle;

            detail("Set session handle to %"PRIu32".", eip_pdu_header.session_handle);

            detail("Encoding EIP header.");
            slice_rc = encode_eip_pdu(&response_header_slice, &eip_pdu_header, &response_payload_slice);
            if(slice_rc != STATUS_OK) {
                warn("Unable to encode response header slice!");
                rc = STATUS_ERR_OP_FAILED;
                break;
            }
        }
    } while(0);

    if(rc == STATUS_OK) {
        /* if there is a parent_response delay requested, then wait a bit. */
        if(connection->response_delay > 0) {
            util_sleep_ms(connection->response_delay);
        }
    }

    return rc;
}



status_t register_session(slice_p request, slice_p response, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    uint8_t *saved_start = response->start;

    struct {
        uint16_t eip_version;
        uint16_t option_flags;
    } register_request;

    do {
        uint32_t offset = 0;

        GET_FIELD(request, u16, &(register_request.eip_version), sizeof(register_request.eip_version));
        GET_FIELD(request, u16, &(register_request.option_flags), sizeof(register_request.option_flags));

        /* session_handle must be zero. */
        assert_warn((connection->session_handle == (uint32_t)0), STATUS_ERR_PARAM, "Request failed sanity check: request session handle is %04"PRIx32" but should be zero.", connection->session_handle);

        /* request EIP version must be 1 (one). */
        assert_warn((register_request.eip_version == (uint32_t)1), STATUS_ERR_PARAM, "Request failed sanity check: request EIP version is %04"PRIx32" but should be one (1).", register_request.eip_version);

        /* request option flags must be zero. */
        assert_warn((register_request.option_flags == (uint32_t)0), STATUS_ERR_PARAM, "Request failed sanity check: request option flags is %04"PRIx32" but should be zero.", register_request.option_flags);

        /* all good, generate a session handle. */
        connection->session_handle = (uint32_t)rand();

        /* encode the response */
        offset = 0;
        SET_FIELD(response, u16, register_request.eip_version, sizeof(register_request.eip_version));
        SET_FIELD(response, u16, register_request.option_flags, sizeof(register_request.option_flags));

        if(!slice_truncate_to_offset(response, offset)) {
            warn("Unable to truncate response!");
            rc = STATUS_ERR_OP_FAILED;
            break;
        }
    } while(0);


    return rc;
}



status_t unregister_session(slice_p request, slice_p response, plc_connection_p connection)
{
    (void)request;
    (void)response;
    (void)connection;

    /* shut down the connection */
    return STATUS_TERMINATE;
}



status_t decode_eip_pdu(slice_p request, eip_pdu_header_p eip_pdu_header, slice_p request_header_slice, slice_p request_payload_slice)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;

        assert_warn((eip_pdu_header), STATUS_ERR_NULL_PTR, "Header pointer is null!");

        assert_detail((slice_get_len(request) >= EIP_HEADER_SIZE), STATUS_ERR_RESOURCE, "Not enough data for the EIP eip_pdu_header.");

        memset(eip_pdu_header, 0, sizeof(*eip_pdu_header));

        GET_FIELD(request, u16, &(eip_pdu_header->command), sizeof(eip_pdu_header->command));
        GET_FIELD(request, u16, &(eip_pdu_header->length), sizeof(eip_pdu_header->length));
        GET_FIELD(request, u32, &(eip_pdu_header->session_handle), sizeof(eip_pdu_header->session_handle));
        GET_FIELD(request, u32, &(eip_pdu_header->status), sizeof(eip_pdu_header->status));
        GET_FIELD(request, u64, &(eip_pdu_header->sender_context), sizeof(eip_pdu_header->sender_context));
        GET_FIELD(request, u32, &(eip_pdu_header->options), sizeof(eip_pdu_header->options));

        rc = slice_split_at_offset(request, offset, request_header_slice, request_payload_slice);
        if(rc != STATUS_OK) {
            warn("Unable to split request slice into header and payload parts!");
            break;
        }
    } while(0);

    return rc;
}




status_t encode_eip_pdu(slice_p pdu_header_slice, eip_pdu_header_p eip_pdu_header, slice_p response_payload)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;

        assert_detail((slice_get_len(pdu_header_slice) >= EIP_HEADER_SIZE), STATUS_ERR_RESOURCE, "Not enough space for the EIP PDU header.");

        /* encode EIP eip_pdu_header. */
        SET_FIELD(pdu_header_slice, u16, eip_pdu_header->command, sizeof(eip_pdu_header->command));
        SET_FIELD(pdu_header_slice, u16, (uint16_t)slice_get_len(response_payload), sizeof(eip_pdu_header->length));
        SET_FIELD(pdu_header_slice, u32, eip_pdu_header->session_handle, sizeof(eip_pdu_header->session_handle));
        SET_FIELD(pdu_header_slice, u32, eip_pdu_header->status, sizeof(eip_pdu_header->status));
        SET_FIELD(pdu_header_slice, u64, eip_pdu_header->sender_context, sizeof(eip_pdu_header->sender_context));
        SET_FIELD(pdu_header_slice, u32, eip_pdu_header->options, sizeof(eip_pdu_header->options));
    } while(0);

    return rc;
}


status_t reserve_eip_pdu(slice_p response, slice_p pdu_header_slice, slice_p pdu_payload_slice)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t header_size = 0;
        eip_pdu_header_t eip_pdu_header;

        assert_warn((slice_get_len(response) >= EIP_HEADER_SIZE), STATUS_ERR_RESOURCE, "Insuffienct space in response buffer for EIP PDU header!");

        header_size = 0;
        header_size += sizeof(eip_pdu_header.command);
        header_size += sizeof(eip_pdu_header.length);
        header_size += sizeof(eip_pdu_header.session_handle);
        header_size += sizeof(eip_pdu_header.status);
        header_size += sizeof(eip_pdu_header.sender_context);
        header_size += sizeof(eip_pdu_header.options);

        rc = slice_split_at_offset(response, header_size, pdu_header_slice, pdu_payload_slice);
    } while(0);

    return rc;
}
