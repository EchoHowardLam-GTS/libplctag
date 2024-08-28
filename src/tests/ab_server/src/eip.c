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
#include "cip.h"
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



static status_t check_eip_header(slice_p encoded_pdu_data, eip_header_p header);
static status_t process_register_session_request(slice_p encoded_pdu_data, plc_connection_p connection);
static status_t process_unregister_session_request(slice_p encoded_pdu_data, plc_connection_p connection);
static status_t process_unconnected_header(slice_p encoded_pdu_data, plc_connection_p connection);
static status_t process_connected_header(slice_p encoded_pdu_data, plc_connection_p connection);


status_t eip_process_pdu(slice_p encoded_pdu_data, app_connection_data_p app_connection_data, app_data_p app_data)
{
    status_t rc = STATUS_OK;
    plc_connection_p connection = (plc_connection_p)app_connection_data;
    uint32_t pdu_start = 0;
    uint16_t eip_command;
    eip_header_t header = {0};

    (void)app_data;

    detail("Starting.");

    info("got packet:");
    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(encoded_pdu_data), slice_get_end_ptr(encoded_pdu_data));

    do {
        /* check the encoded PDU length and values */
        if((rc = check_eip_header(encoded_pdu_data, &header)) != STATUS_OK) {
            break;
        }

        eip_command = le2h(header.command);

        switch(eip_command) {
            case EIP_CONNECTED_SEND:
                rc = process_connected_header(encoded_pdu_data, connection);
                break;

            case EIP_UNCONNECTED_SEND:
                rc = process_unconnected_header(encoded_pdu_data, connection);
                break;

            case EIP_REGISTER_SESSION:
                rc = process_register_session_request(encoded_pdu_data, connection);
                break;

            case EIP_UNREGISTER_SESSION:
                rc = process_unregister_session_request(encoded_pdu_data, connection);
                break;

            default:
                warn("Got EIP unknown command %"PRIx16"!", eip_command);
                rc = STATUS_NOT_RECOGNIZED;
                break;
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
    } else {
        if(rc != STATUS_TERMINATE) {
            warn("EIP processing failed with error %s!", status_to_str(rc));


            /* encode an EIP error response */
            header.status = h2le32(translate_to_eip_status(rc));

            /* cap the packet size at the end of the EIP packet. */
            slice_set_start(encoded_pdu_data, 0);
            slice_set_end(encoded_pdu_data, sizeof(eip_header_t));
            header.length = h2le16(0);
        }
    }

    detail("Done with status %s.", status_to_str(rc));

    return rc;
}


status_t check_eip_header(slice_p encoded_pdu_data, eip_header_p header)
{
    status_t rc = STATUS_OK;

    do {
        size_t data_len = slice_get_len(encoded_pdu_data);

        if(data_len < sizeof(*header)) {
            /* not enough data */
            detail("Not enough data read yet.  Get more.");
            rc = STATUS_PARTIAL;
            break;
        }

        header = (eip_header_p)slice_get_start_ptr(encoded_pdu_data);

        if(data_len != sizeof(*header) + le2h16(header->length)) {
            warn("PDU is wrong length!  Expected %zu bytes but got %zu bytes!", (sizeof(*header) + le2h16(header->length)), data_len);
            rc = STATUS_BAD_INPUT;
            break;
        }
    } while(0);

    return rc;
}






status_t process_register_session_request(slice_p encoded_pdu_data, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    uint32_t response_start = 0;

    do {
        eip_register_session_request_pdu_t *request = NULL;
        size_t pdu_len = slice_get_len(encoded_pdu_data);

        /* this cannot be a second session registration request. */
        assert_warn((connection->session_handle == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: EIP connection is already registered with session handle %"PRIx32"!", connection->session_handle);

        /* check the size */
        if(pdu_len != sizeof(eip_register_session_request_pdu_t)) {
            warn("Expected %zu bytes for session registration request but got %zu bytes instead!", sizeof(eip_register_session_request_pdu_t), pdu_len);
            rc = STATUS_BAD_INPUT;
            break;
        }

        /* overlay the data. */
        request = (eip_register_session_request_pdu_t *)slice_get_start(encoded_pdu_data);

        /* the session handle in the request must be zero . */
        assert_warn((le2h32(request->eip_header.session_handle) == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: A registration request must have a session handle of zero but we got %"PRIx32"!", le2h32(request->eip_header.session_handle));

        /* pdu EIP version must be 1 (one). */
        assert_warn((le2h16(request->register_session_args.eip_version) == (uint16_t)EIP_VERSION),
                    STATUS_BAD_INPUT,
                    "Request failed sanity check: pdu EIP version is %"PRIu16" but should be one %u.",
                    le2h16(request->register_session_args.eip_version),
                    EIP_VERSION);

        /* pdu option flags must be zero. */
        assert_warn((le2h16(request->register_session_args.option_flags) == (uint16_t)0),
                    STATUS_BAD_INPUT,
                    "Request failed sanity check: pdu option flags is %04"PRIx16" but should be zero.",
                    le2h16(request->register_session_args.option_flags));

        /* all good, generate a session handle. */
        connection->session_handle = (uint32_t)rand();

        /* store it into the decoded PDU.  It will get put into the encoded data when the EIP header is encoded. */
        request->eip_header.session_handle = h2le32(connection->session_handle);

        /* set the length of the encoded data. */
        if(!slice_set_len(encoded_pdu_data, (uint32_t)sizeof(*request))) {
            rc = slice_get_status(encoded_pdu_data);
            warn("Error %s trying to set the end of the encoded data slice!", status_to_str(rc));
            break;
        }
    } while(0);

    return rc;
}



status_t process_unregister_session_request(slice_p encoded_pdu_data, plc_connection_p connection)
{
    (void)encoded_pdu_data;
    (void)connection;

    /* shut down the connection */
    return STATUS_TERMINATE;
}


status_t process_unconnected_header(slice_p encoded_pdu_data, plc_connection_p connection)
{
    status_t rc = STATUS_OK;

    do {
        eip_cpf_unconn_header_t *header = NULL;
        size_t pdu_len = slice_get_len(encoded_pdu_data);
        size_t cip_payload_len = 0;

        /* check the size */
        if(pdu_len > sizeof(*header)) {
            warn("Expected at least %zu bytes for session registration request but got %zu bytes instead!", sizeof(*header), pdu_len);
            rc = STATUS_BAD_INPUT;
            break;
        }

        /* overlay the data. */
        header = (eip_cpf_unconn_header_t *)slice_get_start(encoded_pdu_data);

        /* the session handle must match the one in the connection data. */
        assert_warn((le2h32(header->eip_header.session_handle) == connection->session_handle), STATUS_BAD_INPUT, "Request failed sanity check: The session handle, %"PRIx32" does not match the one for this connection, %"PRIx32"!", le2h32(header->eip_header.session_handle), connection->session_handle);

        /* check the lengths */
        cip_payload_len = le2h16(header->cpf_unconn_header.item_data_length);

        assert_warn((cip_payload_len == pdu_len - sizeof(*header)), STATUS_BAD_INPUT, "Request failed sanity check: The actual CIP payload length, %zu, does not match the CPF data item length, %zu!", (pdu_len - sizeof(*header)), cip_payload_len);

        /* OK, all good, set the start to consume the whole header. */
        if(!slice_set_start(encoded_pdu_data, (uint32_t)sizeof(*header))) {
            rc = slice_get_status(encoded_pdu_data);
            break;
        }

        /*
         * Process the CIP request.  If this fails then something is really wrong.
         * We should get a valid CIP error response if the CIP request is malformed or unrecognized.
         */
        rc = cip_process_request(encoded_pdu_data, connection);
        if(rc != STATUS_OK) {
            warn("Error %s returned trying to process the encapsulated CIP request!", status_to_str(rc));
            break;
        }

        /* set the various length fields. */
        header->cpf_unconn_header.item_data_length = h2le16(slice_get_len(encoded_pdu_data) - sizeof(*header));
        header->eip_header.length = h2le16(slice_get_len(encoded_pdu_data) - sizeof(eip_header_t));
    } while(0);

    return rc;
}



status_t process_connected_header(slice_p encoded_pdu_data, plc_connection_p connection)
{
    status_t rc = STATUS_OK;

    do {
        eip_cpf_conn_header_t *header = NULL;
        size_t pdu_len = slice_get_len(encoded_pdu_data);
        size_t cip_payload_len = 0;

        /* check the size */
        if(pdu_len > sizeof(*header)) {
            warn("Expected at least %zu bytes for session registration request but got %zu bytes instead!", sizeof(*header), pdu_len);
            rc = STATUS_BAD_INPUT;
            break;
        }

        /* overlay the data. */
        header = (eip_cpf_unconn_header_t *)slice_get_start(encoded_pdu_data);

        /* the session handle must match the one in the connection data. */
        assert_warn((le2h32(header->eip_header.session_handle) == connection->session_handle), STATUS_BAD_INPUT, "Request failed sanity check: The session handle, %"PRIx32" does not match the one for this connection, %"PRIx32"!", le2h32(header->eip_header.session_handle), connection->session_handle);

        /* the connection ID must match the one in the connection data. */
        assert_warn((le2h32(header->cpf_conn_header.conn_id) == connection->client_connection_id), STATUS_BAD_INPUT, "Request failed sanity check: The client connection ID, %"PRIx32" in the request does not match the one for this connection, %"PRIx32"!", le2h32(header->cpf_conn_header.conn_id), connection->client_connection_id);

        /* FIXME: we should probably do something to check the sequence number.*/

        /* check the lengths */

        /* the payload size in the CPF header includes the size of the connection sequence number too! */
        cip_payload_len = le2h16(header->cpf_conn_header.item_data_length) - sizeof(header->cpf_conn_header.conn_seq);

        assert_warn((cip_payload_len == pdu_len - sizeof(*header)), STATUS_BAD_INPUT, "Request failed sanity check: The actual CIP payload length, %zu, does not match the CPF data item length, %zu!", (pdu_len - sizeof(*header)), cip_payload_len);

        /* OK, all good, set the start to consume the whole header. */
        if(!slice_set_start(encoded_pdu_data, (uint32_t)sizeof(*header))) {
            rc = slice_get_status(encoded_pdu_data);
            break;
        }

        /*
         * Process the CIP request.  If this fails then something is really wrong.
         * We should get a valid CIP error response if the CIP request is malformed or unrecognized.
         */
        rc = cip_process_request(encoded_pdu_data, connection);
        if(rc != STATUS_OK) {
            warn("Error %s returned trying to process the encapsulated CIP request!", status_to_str(rc));
            break;
        }

        /* set the connection ID and the sequence ID */
        connection->server_connection_seq++;

        header->cpf_conn_header.conn_seq = h2le16(connection->server_connection_seq);
        header->cpf_conn_header.conn_id = h2le32(connection->server_connection_id);

        /* set the various length fields. */
        header->cpf_conn_header.item_data_length = h2le16((uint16_t)(sizeof(header->cpf_conn_header.conn_seq) + slice_get_len(encoded_pdu_data) - sizeof(*header)));
        header->eip_header.length = h2le16(slice_get_len(encoded_pdu_data) - sizeof(eip_header_t));
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


// status_t encode_eip_error_pdu(slice_p pdu, eip_header_p header, status_t status)
// {
//     status_t rc = STATUS_OK;

//     info("Starting with input status %s.", status_to_str(status));

//     do {
//         /* override the start offset. */
//         if(!slice_set_start(pdu, 0)) {
//             rc = slice_get_status(pdu);
//             warn("Error %s trying to set start to zero on the error PDU!", status_to_str(rc));
//             break;
//         }

//         /* override the PDU length */
//         if(!slice_set_len(pdu, EIP_HEADER_SIZE)) {
//             rc = slice_get_status(pdu);
//             warn("Error %s trying to set the error PDU size!", status_to_str(rc));
//             break;
//         }

//         /* set the status PDU field */
//         header->status = h2le32(translate_to_eip_status(status));

//         rc = encode_eip_header(pdu, header, connection);
//     } while(0);

//     info("Done with status %s.", status_to_str(rc));

//     return rc;
// }
