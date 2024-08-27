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



// static status_t encode_eip_error_pdu(slice_p pdu, eip_header_p header, status_t status);
// static eip_status_t translate_to_eip_status(status_t status);


static status_t decode_eip_header(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection);


static status_t encode_eip_header(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection);
// static status_t reserve_eip_pdu(slice_p response, slice_p pdu_header_slice, slice_p eip_header_p);




static status_t process_register_session_request(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection);
static status_t process_unregister_session_request(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection);





status_t eip_process_pdu(slice_p encoded_pdu_data, app_connection_data_p app_connection_data, app_data_p app_data)
{
    status_t rc = STATUS_OK;
    plc_connection_p connection = (plc_connection_p)app_connection_data;
    uint32_t pdu_start = 0;
    eip_pdu_t decoded_pdu = {0};

    (void)app_data;

    detail("Starting.");

    info("got packet:");
    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(encoded_pdu_data), slice_get_end_ptr(encoded_pdu_data));

    do {
        uint32_t session_handle = 0;
        uint32_t request_len = slice_get_len(encoded_pdu_data);

        if(request_len < EIP_HEADER_SIZE) {
            /* make sure that the size of the buffer is correct. */

            detail("PDU has %"PRIu32" bytes of data but needs %"PRIu32" bytes of data.", request_len, (uint32_t)(EIP_HEADER_SIZE));

            if(slice_set_end_delta(encoded_pdu_data, (EIP_HEADER_SIZE - request_len))) {
                rc = STATUS_PARTIAL;
                break;
            } else {
                rc = slice_get_status(encoded_pdu_data);
                warn("Error setting end offset in encoded request data.");
                break;
            }
        }

        /* parse the PDU EIP header and set the start of the request past that. */
        rc = decode_eip_header(encoded_pdu_data, &decoded_pdu, connection);
        if(rc != STATUS_OK) {
            warn("Got error %s attempting to decode the EIP PDU!", status_to_str(rc));
            break;
        }

        /* If we have a session handle already, we must match it with the incoming encoded_pdu_data */
        assert_warn(((!connection->session_handle) || (le2h32(decoded_pdu.eip_header.session_handle) == connection->session_handle)),
                     STATUS_BAD_INPUT,
                     "Request session handle %08"PRIx32" does not match the one for this connection, %08"PRIx32"!",
                     le2h32(decoded_pdu.eip_header.session_handle),
                     connection->session_handle
                    );

        /* dispatch the encoded_pdu_data */
        switch(le2h16(decoded_pdu.eip_header.command)) {
            case EIP_REGISTER_SESSION:
                rc = process_register_session_request(encoded_pdu_data, &decoded_pdu, connection);
                break;

            case EIP_UNREGISTER_SESSION:
                rc = process_unregister_session_request(encoded_pdu_data, &decoded_pdu, connection);
                break;

            case EIP_CONNECTED_SEND:
                rc = cpf_dispatch_connected_request(encoded_pdu_data, &decoded_pdu, connection);
                break;

            case EIP_UNCONNECTED_SEND:
                rc = cpf_dispatch_unconnected_request(encoded_pdu_data, &decoded_pdu, connection);
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
            detail("EIP response has a payload of %"PRIu32" bytes.", slice_get_len(encoded_pdu_data));

            detail("Encoding EIP header.");
            rc = encode_eip_header(encoded_pdu_data, &decoded_pdu, connection);
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



status_t process_register_session_request(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    uint32_t response_start = 0;


    do {
        uint32_t pdu_length = 0;

        assert_warn((encoded_pdu_data), STATUS_NULL_PTR, "Encoded data pointer is null!");
        assert_warn((decoded_pdu), STATUS_NULL_PTR, "Decoded PDU pointer is null!");

        /* the start index is set by the previous stage. */

        pdu_length = slice_get_len(encoded_pdu_data);
        rc = slice_get_status(encoded_pdu_data);
        if(rc != STATUS_OK) {
            warn("Error %s getting the encoded data length!", status_to_str(rc));
            break;
        }

        if(pdu_length < sizeof(decoded_pdu->eip_command_header.register_session_request)) {
            info("PDU length is less than the size of the EIP header.");
            rc = STATUS_PARTIAL;
            break;
        }

        /* overlay the EIP header on the data. */
        decoded_pdu->eip_command_header.register_session_request = *(eip_register_session_p)slice_get_start_ptr(encoded_pdu_data);

        /* session_handle must be zero. */
        assert_warn((connection->session_handle == (uint32_t)0), STATUS_BAD_INPUT, "Request failed sanity check: pdu session handle is %04"PRIx32" but should be zero.", connection->session_handle);

        /* pdu EIP version must be 1 (one). */
        assert_warn((le2h16(decoded_pdu->eip_command_header.register_session_request.eip_version) == (uint16_t)1),
                    STATUS_BAD_INPUT,
                    "Request failed sanity check: pdu EIP version is %04"PRIx16" but should be one (1).",
                    le2h16(decoded_pdu->eip_command_header.register_session_request.eip_version));

        /* pdu option flags must be zero. */
        assert_warn((le2h16(decoded_pdu->eip_command_header.register_session_request.option_flags) == (uint16_t)0),
                    STATUS_BAD_INPUT,
                    "Request failed sanity check: pdu option flags is %04"PRIx16" but should be zero.",
                    le2h16(decoded_pdu->eip_command_header.register_session_request.option_flags));

        /* all good, generate a session handle. */
        connection->session_handle = (uint32_t)rand();

        /* store it into the decoded PDU.  It will get put into the encoded data when the EIP header is encoded. */
        decoded_pdu->eip_header.session_handle = h2le32(connection->session_handle);

        /* set the end of the encoded data. */
        if(!slice_set_end(encoded_pdu_data, slice_get_start(encoded_pdu_data) + (uint32_t)sizeof(eip_register_session_t))) {
            rc = slice_get_status(encoded_pdu_data);
            warn("Error %s trying tos et the end of the encoded data slice!", status_to_str(rc));
            break;
        }
    } while(0);

    return rc;
}



status_t process_unregister_session_request(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection)
{
    (void)encoded_pdu_data;
    (void)decoded_pdu;
    (void)connection;

    /* shut down the connection */
    return STATUS_TERMINATE;
}



status_t decode_eip_header(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t pdu_length = 0;
        uint32_t eip_payload_length = 0;

        assert_warn((encoded_pdu_data), STATUS_NULL_PTR, "Encoded data pointer is null!");
        assert_warn((decoded_pdu), STATUS_NULL_PTR, "Decoded PDU pointer is null!");

        /* make sure we are starting at the beginning of the PDU. */
        if(!slice_set_start(encoded_pdu_data, 0)) {
            rc = slice_get_status(encoded_pdu_data);
            warn("Error %s trying to set start to zero on the encoded data!", status_to_str(rc));
            break;
        }

        pdu_length = slice_get_len(encoded_pdu_data);
        rc = slice_get_status(encoded_pdu_data);
        if(rc != STATUS_OK) {
            warn("Error %s getting the encoded data length!", status_to_str(rc));
            break;
        }

        if(pdu_length < EIP_HEADER_SIZE) {
            info("PDU length is less than the size of the EIP header.");
            rc = STATUS_PARTIAL;
            break;
        }

        /* overlay the EIP header on the data. */
        decoded_pdu->eip_header = *(eip_header_p)slice_get_start_ptr(encoded_pdu_data);

        /* get the length and check against the encoded PDU size. */
        eip_payload_length = le2h16(decoded_pdu->eip_header.length);

        if(pdu_length < sizeof(decoded_pdu->eip_header) + eip_payload_length) {
            info("Not enough data, read more from the socket.");
            rc = STATUS_PARTIAL;
            break;
        }

        if(pdu_length > sizeof(decoded_pdu->eip_header) + eip_payload_length) {
            warn("Too much data! Expected %"PRIu32" bytes in the encoded request. Got %"PRIu32" bytes.", (uint32_t)(sizeof(decoded_pdu->eip_header) + eip_payload_length), pdu_length);
            rc = STATUS_BAD_INPUT;
            break;
        }

        /* set up for the rest of the processing */
        if(!slice_set_start(encoded_pdu_data, (uint32_t)sizeof(decoded_pdu->eip_header))) {
            rc = slice_get_status(encoded_pdu_data);
            warn("Error %s trying to set start after the header in the PDU!", status_to_str(rc));
            break;
        }
    } while(0);

    return rc;
}




status_t encode_eip_header(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t pdu_length;
        eip_header_p eip_header = NULL;

        assert_warn((encoded_pdu_data), STATUS_NULL_PTR, "Encoded data pointer is null!");
        assert_warn((decoded_pdu), STATUS_NULL_PTR, "Decoded PDU pointer is null!");

        /* make sure we are starting at the beginning of the PDU. */
        if(!slice_set_start(encoded_pdu_data, 0)) {
            rc = slice_get_status(encoded_pdu_data);
            warn("Error %s trying to set start to zero on the encoded data!", status_to_str(rc));
            break;
        }

        pdu_length = slice_get_len(encoded_pdu_data);
        rc = slice_get_status(encoded_pdu_data);
        if(rc != STATUS_OK) {
            warn("Error %s getting the encoded data length!", status_to_str(rc));
            break;
        }

        /* overlay the EIP header on the data. */
        eip_header = (eip_header_p)slice_get_start_ptr(encoded_pdu_data);

        /* update the length */
        eip_header->length = h2le16((uint16_t)pdu_length - (uint16_t)sizeof(*eip_header));

        /* update the session handle and the session context */
        eip_header->session_handle = h2le32(connection->session_handle);
        eip_header->sender_context = h2le64(connection->sender_context);
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
