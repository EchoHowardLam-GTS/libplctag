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

/* supported EIP version */
const uint16_t EIP_VERSION     = ((uint16_t)1);

/* EIP header size is 24 bytes. */
const uint32_t EIP_HEADER_SIZE = (24);

const uint16_t EIP_REGISTER_SESSION_SIZE  = ((uint16_t)4); /* 4 bytes, 2 16-bit words */



typedef struct {
    struct pdu_t;

    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
    uint64_t sender_context;
    uint32_t options;
} eip_pdu_t;

typedef eip_pdu_t *eip_pdu_p;






static status_t decode_eip_pdu(slice_p request, slice_p response, eip_pdu_p pdu);
static status_t encode_eip_pdu(eip_pdu_p pdu);
// static status_t reserve_eip_pdu(slice_p response, slice_p pdu_header_slice, slice_p eip_pdu_p);




static status_t register_session(slice_p request, slice_p response, plc_connection_p connection);
static status_t unregister_session(slice_p request, slice_p response, plc_connection_p connection);


status_t eip_process_request(slice_p request, slice_p response, app_connection_data_p app_connection_data, app_data_p app_data)
{
    status_t rc = STATUS_OK;
    plc_connection_p connection = (plc_connection_p)app_connection_data;
    uint8_t *saved_start = NULL;
    slice_t request_header_slice = {0};
    slice_t response_header_slice = {0};
    eip_pdu_t pdu = {0};

    (void)app_data;

    info("got packet:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        uint32_t session_handle = 0;

        assert_detail((slice_get_len(request) >= EIP_HEADER_SIZE), STATUS_NO_RESOURCE, "Not enough data for the EIP request PDU.");

        rc = decode_eip_pdu(request, response, &pdu);
        if(rc != STATUS_OK) {
            warn("Got error %s attempting to decode the EIP PDU!", status_to_str(rc));
            break;
        }

        /* If we have a session handle already, we must match it with the incoming request */
        assert_warn(((!connection->session_handle) || (pdu.session_handle == connection->session_handle)),
                     STATUS_BAD_INPUT,
                     "Request session handle %08"PRIx32" does not match the one for this connection, %08"PRIx32"!",
                     pdu.session_handle,
                     connection->session_handle
                    );

        /* set up the response slices */
        rc = slice_split_at_offset(response, EIP_HEADER_SIZE, &(pdu.response_header_slice), &(pdu.response_payload_slice));
        if(rc != STATUS_OK) {
            warn("Error %s trying to reserve response header and payload slices!", status_to_str(rc));
            rc = STATUS_SETUP_FAILURE;
            break;
        }

        /* dispatch the request */
        switch(pdu.command) {
            case EIP_REGISTER_SESSION:
                rc = register_session(&(pdu.request_payload_slice), &(pdu.response_payload_slice), connection);
                break;

            case EIP_UNREGISTER_SESSION:
                rc = unregister_session(&(pdu.request_payload_slice), &(pdu.response_payload_slice), connection);
                break;

            case EIP_CONNECTED_SEND:
                rc = cpf_dispatch_connected_request(&(pdu.request_payload_slice), &(pdu.response_payload_slice), connection);
                break;

            case EIP_UNCONNECTED_SEND:
                rc = cpf_dispatch_unconnected_request(&(pdu.request_payload_slice), &(pdu.response_payload_slice), connection);
                break;

            default:
                rc = STATUS_NOT_RECOGNIZED;
                break;
        }

        /* check the status for fatal errors */
        if(status_is_error(rc)) {
            warn("Terminating due to error %s!", status_to_str(rc));
            rc = STATUS_TERMINATE;
            break;
        }

        /* are we terminating the connection or so broken that we need to? */
        if(rc == STATUS_TERMINATE) {
            break;
        }

        /* build up the response if everything was good. */
        if(rc == STATUS_OK) {
            detail("EIP response has a payload of %"PRIu32" bytes.", slice_get_len(&(pdu.response_payload_slice)));

            /* truncate the response to the header plus payload */
            if((rc = slice_truncate_to_offset(response, slice_get_len(&(pdu.response_payload_slice)) + slice_get_len(&response_header_slice))) != STATUS_OK) {
                warn("Got error %s attempting to truncate response slice!", status_to_str(rc));
                break;
            }

            /* set the session handle */
            pdu.session_handle = connection->session_handle;

            detail("Set session handle to %"PRIu32".", pdu.session_handle);

            detail("Encoding EIP header.");
            rc = encode_eip_pdu(&pdu);
            if(rc != STATUS_OK) {
                warn("Error %s encoding response header slice!", status_to_str(rc));
                break;
            }
        }
    } while(0);

    if(rc == STATUS_OK) {
        /* if there is a parent_response delay requested, then wait a bit. */
        if(connection->response_delay > 0) {
            int64_t total_delay = connection->response_delay;
            uint32_t step_delay = (total_delay < 50) ? total_delay : 50;

            detail("Debugging response delay %"PRIu32" but step delay is %"PRIu32".", connection->response_delay, step_delay);

            /* wait in small chunks unless we are terminating */
            while(total_delay > 0 && !program_terminating(connection)) {
                util_sleep_ms(step_delay);

                total_delay -= step_delay;
            }
        }
    }

    if(program_terminating(connection)) {
        info("Aborting due to program termination.");
        rc = STATUS_ABORTED;
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
        assert_warn((connection->session_handle == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: request session handle is %04"PRIx32" but should be zero.", connection->session_handle);

        /* request EIP version must be 1 (one). */
        assert_warn((register_request.eip_version == (uint32_t)1), STATUS_BAD_INPUT, "Request failed sanity check: request EIP version is %04"PRIx32" but should be one (1).", register_request.eip_version);

        /* request option flags must be zero. */
        assert_warn((register_request.option_flags == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: request option flags is %04"PRIx32" but should be zero.", register_request.option_flags);

        /* all good, generate a session handle. */
        connection->session_handle = (uint32_t)rand();

        /* encode the response */
        offset = 0;
        SET_FIELD(response, u16, register_request.eip_version, sizeof(register_request.eip_version));
        SET_FIELD(response, u16, register_request.option_flags, sizeof(register_request.option_flags));

        if((rc = slice_truncate_to_offset(response, offset)) != STATUS_OK) {
            warn("Unable to truncate response! Error: %s", status_to_str(rc));
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



status_t decode_eip_pdu(slice_p request, slice_p response, eip_pdu_p pdu)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;
        uint32_t payload_length = 0;

        assert_warn((pdu), STATUS_NULL_PTR, "Header pointer is null!");

        assert_detail((slice_get_len(request) >= EIP_HEADER_SIZE), STATUS_NO_RESOURCE, "Not enough data for the EIP pdu.");

        memset(pdu, 0, sizeof(*pdu));

        GET_FIELD(request, u16, &(pdu->command), sizeof(pdu->command));
        GET_FIELD(request, u16, &(pdu->length), sizeof(pdu->length));
        GET_FIELD(request, u32, &(pdu->session_handle), sizeof(pdu->session_handle));
        GET_FIELD(request, u32, &(pdu->status), sizeof(pdu->status));
        GET_FIELD(request, u64, &(pdu->sender_context), sizeof(pdu->sender_context));
        GET_FIELD(request, u32, &(pdu->options), sizeof(pdu->options));

        rc = slice_split_at_offset(request, offset, &(pdu->request_header_slice), &(pdu->request_payload_slice));
        if(rc != STATUS_OK) {
            warn("Error %s splitting out request payload!", status_to_str(rc));
            break;
        }

        payload_length = slice_get_len(&(pdu->request_payload_slice));

        if(pdu->length > payload_length) {
            info("We need to get more data to get the full EIP PDU.");
            rc = STATUS_PARTIAL;
            break;
        }

        if(pdu->length < payload_length) {
            warn("Unexpected extra data at the end of the PDU!");
            rc = STATUS_BAD_INPUT;
            break;
        }
    } while(0);

    return rc;
}




status_t encode_eip_pdu(eip_pdu_p pdu)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;

        assert_detail((slice_get_len(&(pdu->response_header_slice)) >= EIP_HEADER_SIZE), STATUS_NO_RESOURCE, "Not enough space for the EIP PDU header.");

        /* encode EIP pdu. */
        SET_FIELD(&(pdu->response_header_slice), u16, pdu->command, sizeof(pdu->command));
        SET_FIELD(&(pdu->response_header_slice), u16, (uint16_t)slice_get_len(&(pdu->response_payload_slice)), sizeof(pdu->length));
        SET_FIELD(&(pdu->response_header_slice), u32, pdu->session_handle, sizeof(pdu->session_handle));
        SET_FIELD(&(pdu->response_header_slice), u32, pdu->status, sizeof(pdu->status));
        SET_FIELD(&(pdu->response_header_slice), u64, pdu->sender_context, sizeof(pdu->sender_context));
        SET_FIELD(&(pdu->response_header_slice), u32, pdu->options, sizeof(pdu->options));
    } while(0);

    return rc;
}


// status_t reserve_eip_pdu(slice_p response, slice_p pdu_header_slice, slice_p pdu_payload_slice)
// {
//     status_t rc = STATUS_OK;

//     do {
//         uint32_t header_size = 0;
//         eip_pdu_t pdu;

//         assert_warn((slice_get_len(response) >= EIP_HEADER_SIZE), STATUS_NO_RESOURCE, "Insuffienct space in response buffer for EIP PDU header!");

//         header_size = 0;
//         header_size += sizeof(pdu.command);
//         header_size += sizeof(pdu.length);
//         header_size += sizeof(pdu.session_handle);
//         header_size += sizeof(pdu.status);
//         header_size += sizeof(pdu.sender_context);
//         header_size += sizeof(pdu.options);

//         rc = slice_split_at_offset(response, header_size, pdu_header_slice, pdu_payload_slice);
//     } while(0);

//     return rc;
// }
