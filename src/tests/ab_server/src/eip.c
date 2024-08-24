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
    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
    uint64_t sender_context;
    uint32_t options;
} eip_header_t;

typedef eip_header_t *eip_header_p;




static status_t encode_eip_error_pdu(slice_p pdu, eip_header_p header, status_t status);
static eip_status_t translate_to_eip_status(status_t status);


static status_t decode_eip_pdu(slice_p pdu, eip_header_p header);
static status_t encode_eip_header(slice_p pdu, eip_header_p header);
// static status_t reserve_eip_pdu(slice_p response, slice_p pdu_header_slice, slice_p eip_header_p);




static status_t register_session(slice_p pdu, plc_connection_p connection);
static status_t unregister_session(slice_p pdu, plc_connection_p connection);


status_t eip_process_pdu(slice_p pdu, app_connection_data_p app_connection_data, app_data_p app_data)
{
    status_t rc = STATUS_OK;
    plc_connection_p connection = (plc_connection_p)app_connection_data;
    uint32_t pdu_start = 0;
    eip_header_t header = {0};

    (void)app_data;

    detail("Starting.");

    info("got packet:");
    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(pdu), slice_get_end_ptr(pdu));

    do {
        uint32_t session_handle = 0;
        uint32_t request_len = slice_get_len(pdu);

        if(request_len < EIP_HEADER_SIZE) {
            /* make sure that the size of the buffer is correct. */

            detail("PDU has %"PRIu32" bytes of data but needs %"PRIu32" bytes of data.", request_len, (uint32_t)(EIP_HEADER_SIZE));

            if(slice_set_end_delta(pdu, (EIP_HEADER_SIZE - request_len))) {
                /* success */
                rc = STATUS_PARTIAL;
                break;
            }
        }

        /* parse the PDU EIP header and set the start of the request past that. */
        rc = decode_eip_pdu(pdu, &header);
        if(rc != STATUS_OK) {
            warn("Got error %s attempting to decode the EIP PDU!", status_to_str(rc));
            break;
        }

        /* If we have a session handle already, we must match it with the incoming pdu */
        assert_warn(((!connection->session_handle) || (header.session_handle == connection->session_handle)),
                     STATUS_BAD_INPUT,
                     "Request session handle %08"PRIx32" does not match the one for this connection, %08"PRIx32"!",
                     header.session_handle,
                     connection->session_handle
                    );

        /* dispatch the pdu */
        switch(header.command) {
            case EIP_REGISTER_SESSION:
                rc = register_session(pdu, connection);
                break;

            case EIP_UNREGISTER_SESSION:
                rc = unregister_session(pdu, connection);
                break;

            case EIP_CONNECTED_SEND:
                rc = cpf_dispatch_connected_request(pdu, connection);
                break;

            case EIP_UNCONNECTED_SEND:
                rc = cpf_dispatch_unconnected_request(pdu, connection);
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
            detail("EIP response has a payload of %"PRIu32" bytes.", slice_get_len(pdu));

            /* set the session handle */
            header.session_handle = connection->session_handle;

            detail("Set session handle to %"PRIu32".", header.session_handle);

            detail("Encoding EIP header.");
            rc = encode_eip_header(pdu, &header);
            if(rc != STATUS_OK) {
                warn("Error %s encoding response EIP header!", status_to_str(rc));
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

    detail("Done with status %s.", status_to_str(rc));

    return rc;
}



status_t register_session(slice_p pdu, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    uint32_t response_start = 0;

    struct {
        uint16_t eip_version;
        uint16_t option_flags;
    } register_request;

    size_t request_payload_size = sizeof(register_request.eip_version) + sizeof(register_request.option_flags);
    uint32_t pdu_length = 0;

    do {
        uint32_t offset = 0;

        response_start = offset = slice_get_start(pdu);
        rc = slice_get_status(pdu);
        if(rc != STATUS_OK) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to get the start offset!", status_to_str(rc));
            break;
        }

        /* make sure the payload is the right length. */
        pdu_length = slice_get_len(pdu);
        rc = slice_get_status(pdu);
        if(rc != STATUS_OK) {
            warn("Error %s getting the PDU length!", status_to_str(rc));
            break;
        }

        if((size_t)pdu_length != request_payload_size) {
            info("PDU length is not the right size!");
            rc = STATUS_PARTIAL;
            break;
        }

        GET_UINT_FIELD(pdu, register_request.eip_version);
        GET_UINT_FIELD(pdu, register_request.option_flags);

        /* session_handle must be zero. */
        assert_warn((connection->session_handle == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: pdu session handle is %04"PRIx32" but should be zero.", connection->session_handle);

        /* pdu EIP version must be 1 (one). */
        assert_warn((register_request.eip_version == (uint32_t)1), STATUS_BAD_INPUT, "Request failed sanity check: pdu EIP version is %04"PRIx32" but should be one (1).", register_request.eip_version);

        /* pdu option flags must be zero. */
        assert_warn((register_request.option_flags == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: pdu option flags is %04"PRIx32" but should be zero.", register_request.option_flags);

        /* all good, generate a session handle. */
        connection->session_handle = (uint32_t)rand();

        /* encode the response */
        offset = response_start;

        SET_UINT_FIELD(pdu, register_request.eip_version);
        SET_UINT_FIELD(pdu, register_request.option_flags);

        /* set the PDU end point */
        if(!slice_set_end(pdu, offset)) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to set the PDU end!", status_to_str(rc));
            break;
        }
    } while(0);


    return rc;
}



status_t unregister_session(slice_p pdu, slice_p response, plc_connection_p connection)
{
    (void)pdu;
    (void)response;
    (void)connection;

    /* shut down the connection */
    return STATUS_TERMINATE;
}



status_t decode_eip_pdu(slice_p pdu, eip_header_p header)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;
        uint32_t pdu_length = 0;

        assert_warn((header), STATUS_NULL_PTR, "Header pointer is null!");

        /* make sure we are starting at the beginning of the PDU. */
        if(!slice_set_start(pdu, 0)) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to set start to zero on the PDU!", status_to_str(rc));
            break;
        }

        pdu_length = slice_get_len(pdu);
        rc = slice_get_status(pdu);
        if(rc != STATUS_OK) {
            warn("Error %s getting the PDU length!", status_to_str(rc));
            break;
        }

        if(pdu_length < EIP_HEADER_SIZE) {
            info("PDU length is less than the size of the EIP header.");
            rc = STATUS_PARTIAL;
            break;
        }

        /* set the header payload size */
        header->length = pdu_length - EIP_HEADER_SIZE;

        /* clear out the header. */
        memset(header, 0, sizeof(*header));

        GET_UINT_FIELD(pdu, header->command);
        GET_UINT_FIELD(pdu, header->length);
        GET_UINT_FIELD(pdu, header->session_handle);
        GET_UINT_FIELD(pdu, header->status);
        GET_UINT_FIELD(pdu, header->sender_context);
        GET_UINT_FIELD(pdu, header->options);

        /* set up for the rest of the processing */
        if(!slice_set_start(pdu, offset)) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to set start after the header in the PDU!", status_to_str(rc));
            break;
        }
    } while(0);

    return rc;
}




status_t encode_eip_header(slice_p pdu, eip_header_p header)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;
        uint32_t pdu_len = 0;

        /* override the boundaries on the PDU. */
        if(!slice_set_start(pdu, 0)) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to set start to zero on the error PDU!", status_to_str(rc));
            break;
        }

        pdu_len = slice_get_len(pdu);
        rc = slice_get_status(pdu);
        if(rc != STATUS_OK) {
            warn("Error %s getting the PDU length!", status_to_str(rc));
            break;
        }

        if(pdu_len < EIP_HEADER_SIZE) {
            warn("The whole EIP PDU is smaller than the EIP header!");
            rc = STATUS_INTERNAL_FAILURE;
            break;
        }

        /* set the size to the payload size */
        header->length = pdu_len - EIP_HEADER_SIZE;

        SET_UINT_FIELD(pdu, header->command);
        SET_UINT_FIELD(pdu, header->length);
        SET_UINT_FIELD(pdu, header->session_handle);
        SET_UINT_FIELD(pdu, header->status);
        SET_UINT_FIELD(pdu, header->sender_context);
        SET_UINT_FIELD(pdu, header->options);
    } while(0);

    return rc;
}


eip_status_t translate_to_eip_status(status_t status)
{
    eip_status_t eip_status = EIP_STATUS_SUCCESS;

    switch(status) {
        case STATUS_ABORTED:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        case STATUS_BAD_INPUT:
            eip_status = EIP_STATUS_BAD_PAYLOAD;
            break;

        case STATUS_BUSY:
            eip_status = EIP_STATUS_NOT_ALLOWED;
            break;

        case STATUS_EXTERNAL_FAILURE:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        case STATUS_INTERNAL_FAILURE:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        case STATUS_NO_RESOURCE:
            eip_status = EIP_STATUS_NOT_ALLOWED;
            break;

        case STATUS_NOT_ALLOWED:
            eip_status = EIP_STATUS_NOT_ALLOWED;
            break;

        case STATUS_NOT_FOUND:
            eip_status = EIP_STATUS_UNSUPPORTED;
            break;

        case STATUS_NOT_RECOGNIZED:
            eip_status = EIP_STATUS_UNSUPPORTED;
            break;

        case STATUS_NOT_SUPPORTED:
            eip_status = EIP_STATUS_UNSUPPORTED;
            break;

        case STATUS_NULL_PTR:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        case STATUS_OK:
            eip_status = EIP_STATUS_SUCCESS;
            break;

        case STATUS_OUT_OF_BOUNDS:
            eip_status = EIP_STATUS_OUT_OF_BOUNDS;
            break;

        case STATUS_PARTIAL:
            eip_status = EIP_STATUS_BAD_PAYLOAD;
            break;

        case STATUS_PENDING:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        case STATUS_SETUP_FAILURE:
            eip_status = EIP_STATUS_BAD_PAYLOAD;
            break;

        case STATUS_TERMINATE:
            eip_status = EIP_STATUS_SUCCESS;
            break;

        case STATUS_TIMEOUT:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        case STATUS_WOULD_BLOCK:
            eip_status = EIP_STATUS_NO_RESOURCE;
            break;

        default:
            warn("Status %d is not known!", status);
            eip_status = EIP_STATUS_UNSUPPORTED;
            break;
    }


    return eip_status;
}


status_t encode_eip_error_pdu(slice_p pdu, eip_header_p header, status_t status)
{
    status_t rc = STATUS_OK;

    info("Starting with input status %s.", status_to_str(status));

    do {
        /* override the start offset. */
        if(!slice_set_start(pdu, 0)) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to set start to zero on the error PDU!", status_to_str(rc));
            break;
        }

        /* override the PDU length */
        if(!slice_set_len(pdu, EIP_HEADER_SIZE)) {
            rc = slice_get_status(pdu);
            warn("Error %s trying to set the error PDU size!", status_to_str(rc));
            break;
        }

        /* set the status PDU field */
        header->status = translate_to_eip_status(status);

        rc = encode_eip_header(pdu, header);
    } while(0);

    info("Done with status %s.", status_to_str(rc));

    return rc;
}
