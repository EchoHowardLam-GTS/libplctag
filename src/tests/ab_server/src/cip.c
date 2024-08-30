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
#include <stdlib.h>
#include "cip.h"
#include "eip.h"
#include "pccc.h"
#include "plc.h"
#include "utils/debug.h"
#include "utils/slice.h"
#include "utils/tcp_server.h"
#include "utils/time_utils.h"



/* path to match. */
// uint8_t LOGIX_CONN_PATH[] = { 0x03, 0x00, 0x00, 0x20, 0x02, 0x24, 0x01 };
// uint8_t MICRO800_CONN_PATH[] = { 0x02, 0x20, 0x02, 0x24, 0x01 };

#define CIP_DONE               ((uint8_t)0x80)

#define CIP_SYMBOLIC_SEGMENT_MARKER ((uint8_t)0x91)


#define CIP_MIN_REQUEST_SIZE (1 + 1 + 4) /* 1 (CIP service), 1 (EPATH length), 4 (class/instance)*/

// typedef struct {
//     uint8_t service_code;   /* why is the operation code _before_ the path? */
//     uint8_t path_size;      /* size in 16-bit words of the path */
//     slice_t path;           /* store this in a slice to avoid copying */
// } cip_header_s;



typedef struct {
    slice_p pdu;
    uint8_t service;
    uint32_t epath_start;
    uint32_t epath_end;
} cip_req_t;

typedef cip_req_t *cip_req_p;



static status_t cip_process_forward_open(slice_p encoded_pdu_data, uint32_t header_len, plc_connection_p connection);
// static status_t process_forward_close(slice_p pdu, plc_connection_p connection);
// static status_t process_tag_read_request(slice_p pdu, plc_connection_p connection);
// static status_t process_tag_write_request(slice_p pdu, plc_connection_p connection);


// static status_t process_tag_segment(tcp_connection_p connection, tag_def_t **tag);
// static status_t process_tag_dim_segment(slice_p tag_path, tag_def_t *tag);

static status_t make_cip_error(slice_p encoded_pdu_data, uint8_t cip_cmd, uint8_t cip_err, int8_t num_extended_err_words, uint16_t *extended_error_words);
static int match_path(slice_p pdu, bool need_pad, uint8_t *path, uint8_t path_len);



status_t cip_process_request(slice_p encoded_pdu_data, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    cip_req_t cip_req;
    uint32_t pdu_start = 0;
    cip_request_header_t *header = NULL;

    info("Got CIP request:");
    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(encoded_pdu_data), slice_get_end_ptr(encoded_pdu_data));

    do {
        uint32_t header_len = 0;

        /* check lengths, this is the minimum */
        assert_warn((slice_get_len(encoded_pdu_data) >= sizeof(cip_request_header_t)), STATUS_NO_RESOURCE, "Insufficient data for a CIP request!");

        /* overlay the header */
        header = (cip_request_header_t *)slice_get_start_ptr(encoded_pdu_data);

        /* calculate the length of the service command plus EPATH. */
        header_len = slice_get_start(encoded_pdu_data) + sizeof(*header) +  (header->epath_word_count * 2);

        /* check that we have room for the epath */
        assert_warn((slice_get_len(encoded_pdu_data) >= header_len), STATUS_NO_RESOURCE, "Insufficient data for the CIP service and EPATH!");

        /* dispatch the request */
        switch(header->service) {
            case CIP_SERVICE_FORWARD_OPEN:
                rc = cip_process_forward_open(encoded_pdu_data, header_len, connection);
                break;

            default:
                warn("Unrecognized CIP service %x!", header->service);
                rc = STATUS_NOT_RECOGNIZED;
                break;
        }

    } while(0);

    /* handle case where we have unimplemented services */
    if(rc != STATUS_OK) {
        rc = make_cip_error(encoded_pdu_data, CIP_ERR_UNSUPPORTED, false, 0, NULL);
    }

    return rc;
}



// status_t decode_cip_request_header(slice_p encoded_pdu_data, eip_pdu_p decoded_pdu, plc_connection_p connection)
// {
//     status_t rc = STATUS_OK;

//     do {
//         uint32_t pdu_start = 0;
//         uint32_t offset = 0;
//         uint32_t request_length = 0;
//         uint8_t epath_word_length = 0;

//         pdu_start = slice_get_start(encoded_pdu_data);
//         if((rc = slice_get_status(encoded_pdu_data)) != STATUS_OK) {
//             warn("Error %s getting PDU start index!");
//             break;
//         }

//         /* save the start */
//         decoded_pdu->cip_payload_offset = pdu_start;

//         /* check the PDU length. */
//         request_length = slice_get_len(encoded_pdu_data);

//         if(request_length < CIP_MIN_REQUEST_SIZE) {
//             warn("Too little data for a valid CIP service request!");
//             rc = STATUS_NO_RESOURCE;
//             break;
//         }

//         offset = pdu_start;

//         /* get the CIP service and set the slice pointing to the EPATH. */
//         decoded_pdu->cip_pdu.cip_request.service = slice_get_uint(encoded_pdu_data, offset, SLICE_BYTE_ORDER_LE, 8);

//         offset++;
//         epath_word_length = slice_get_uint(encoded_pdu_data, offset, SLICE_BYTE_ORDER_LE, 8);
//         offset++;

//         /* check the size again now that we know how long the EPATH is supposed to be. */
//         if(request_length < (uint32_t)(epath_word_length * 2) + (uint32_t)2) {
//             warn("Too little data to contain the CIP service EPATH!");
//             rc = STATUS_NO_RESOURCE;
//             break;
//         }

//         /* set up the epath slice */
//         /* FIXME - check result codes! */
//         slice_init_child(&(decoded_pdu->cip_pdu.cip_request.service_epath), encoded_pdu_data);
//         slice_set_end(&(decoded_pdu->cip_pdu.cip_request.service_epath), offset + (epath_word_length * 2));
//         slice_set_start(&(decoded_pdu->cip_pdu.cip_request.service_epath), offset);

//         /* set the start of the encoded data to show we've consumed the service and path */
//         slice_set_start(encoded_pdu_data, offset);

//         info("CIP request service %x.", decoded_pdu->cip_pdu.cip_request.service);

//         info("Processing CIP PDU with EPATH:");
//         debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(&(decoded_pdu->cip_pdu.cip_request.service_epath)), slice_get_end_ptr(&(decoded_pdu->cip_pdu.cip_request.service_epath)));

//         info("Processing CIP PDU with payload:");
//         debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(encoded_pdu_data), slice_get_end_ptr(encoded_pdu_data));
//     } while(0);

//     return rc;
// }


// /* a handy structure to hold all the parameters we need to receive in a Forward Open request. */
// typedef struct {
//     uint8_t forward_open_service;
//     uint8_t secs_per_tick;                  /* seconds per tick */
//     uint8_t timeout_ticks;                  /* timeout = srd_secs_per_tick * src_timeout_ticks */
//     uint32_t server_conn_id;                /* 0, returned by server in reply. */
//     uint32_t client_conn_id;                /* sent by client. */
//     uint16_t conn_serial_number;            /* client connection ID/serial number */
//     uint16_t orig_vendor_id;                /* client unique vendor ID */
//     uint32_t orig_serial_number;            /* client unique serial number */
//     uint8_t conn_timeout_multiplier;        /* timeout = mult * RPI */
//     uint8_t reserved[3];                    /* reserved, set to 0 */
//     uint32_t client_to_server_rpi;          /* us to target RPI - Request Packet Interval in microseconds */
//     uint32_t client_to_server_conn_params;  /* PDU size from client to us, plus other flags */
//     uint32_t server_to_client_rpi;          /* target to us RPI, in microseconds */
//     uint32_t server_to_client_conn_params;  /* PDU size from us to client, plus other flags */
//     uint8_t transport_class;                /* ALWAYS 0xA3, server transport, class 3, application trigger */
//     slice_t connection_path;                /* connection path. */
// } forward_open_t;

/* the minimal Forward Open with no path */
#define CIP_FORWARD_OPEN_MIN_SIZE   (42)
#define CIP_FORWARD_OPEN_EX_MIN_SIZE   (46)


status_t cip_process_forward_open(slice_p encoded_pdu_data, uint32_t header_len, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    cip_forward_open_request_args_t *fo_args = NULL;
    uint8_t cip_status = CIP_OK;
    uint8_t ext_status_word_count = 0;
    uint16_t ext_status_words[3];

    info("Processing Forward Open request:");
    debug_dump_ptr(DEBUG_INFO, slice_get_start_ptr(encoded_pdu_data), slice_get_end_ptr(encoded_pdu_data));

    do {
        uint32_t required_request_size = 0;

        /* do we have the minimum size? */
        required_request_size = sizeof(*fo_args) + header_len;

        assert_warn((slice_get_len(encoded_pdu_data) >= required_request_size), STATUS_NO_RESOURCE, "Insufficient data for Forward Open request!");

        /* overlay to get the data */
        fo_args = (cip_forward_open_request_args_t *)(slice_get_start_ptr(encoded_pdu_data) + header_len);

        /* check to see how many refusals we should do. */
        if(connection->reject_fo_count > 0) {
            connection->reject_fo_count--;

            info("Forward open request being bounced for debugging. %d to go.", connection->reject_fo_count);

            cip_status = CIP_ERR_COMMS;
            ext_status_word_count = 1;
            ext_status_words[0] = CIP_ERR_EX_DUPLICATE_CONN;
            break;
        }

        /* make sure that there is enough room for the target EPATH */
        required_request_size += (fo_args->target_epath_word_count * 2);

        assert_warn((slice_get_len(encoded_pdu_data) >= required_request_size), STATUS_NO_RESOURCE, "Insufficient data for full Forward Open request!");



        /* Save some values to the persistent connection state. */
        connection->client_connection_id = le2h32(fo_args->orig_to_targ_conn_id);
        connection->client_connection_serial_number = le2h16(o_args->conn_serial_number);
        connection->client_vendor_id = le2h16(fo_args->orig_vendor_id);
        connection->client_serial_number = le2h32(fo_args->orig_serial_number);
        connection->client_to_server_rpi = fo_args->client_to_server_rpi;
        connection->server_to_client_rpi = fo_args->server_to_client_rpi;


        connection->secs_per_tick = fo_args->secs_per_tick;
        connection->timeout_ticks = fo_args->timeout_ticks;
        connection->orig_to_targ_conn_id = le2h32(fo_args->orig_to_targ_conn_id);
        connection->targ_to_orig_conn_id = (uint32_t)rand();
        connection->conn_serial_number = le2h16(fo_args->conn_serial_number);
        connection->orig_vendor_id = le2h16(fo_args->orig_vendor_id);
        connection->orig_serial_number = le2h32(fo_args->orig_serial_number);
        connection->conn_timeout_multiplier = fo_args->conn_timeout_multiplier;
        connection->orig_to_targ_rpi = le2h32(fo_args->orig_to_targ_rpi);
        connection->orig_to_targ_conn_params = le2h16(fo_args->orig_to_targ_conn_params);
        connection->targ_to_orig_rpi = le2h32(fo_args->targ_to_orig_rpi);
        connection->targ_to_orig_conn_params = le2h16(fo_args->targ_to_orig_conn_params);
        connection->transport_class = fo_args->transport_class;

        connection->orig_to_targ_conn_seq = 0;
        connection->targ_to_orig_conn_seq = (uint16_t)rand();


        /* calculate the allowed packet sizes. */
        connection->client_to_server_max_packet = le2h16(fo_args->orig_to_targ_conn_params) & 0x1FF + 64; /* MAGIC */
        connection->server_to_client_max_packet = le2h16(fo_args->targ_to_orig_conn_params) & 0x1FF;


        /* now fill in the response */


        /* FIXME - check that the packet sizes are valid 508 or 4002 */

        /* now process the FO and respond. */
        buf_set_uint8(response, (uint8_t)(fo_cmd | CIP_DONE));
        buf_set_uint8(response, 0); /* padding/reserved. */
        buf_set_uint8(response, CIP_OK); /* no error. */
        buf_set_uint8(response, 0); /* no extra error fields. */

        buf_set_uint32_le(response, client->conn_config.server_connection_id);
        buf_set_uint32_le(response, client->conn_config.client_connection_id);
        buf_set_uint16_le(response, client->conn_config.client_connection_serial_number);
        buf_set_uint16_le(response, client->conn_config.client_vendor_id);
        buf_set_uint32_le(response, client->conn_config.client_serial_number);
        buf_set_uint32_le(response, client->conn_config.client_to_server_rpi);
        buf_set_uint32_le(response, client->conn_config.server_to_client_rpi);

        /* not sure what these do... */
        buf_set_uint8(response, 0);
        buf_set_uint8(response, 0);
    } while(0);

    if(cip_status != CIP_OK) {
        warn("Forward Open request unable to be processed!");

        make_cip_error(encoded_pdu_data,
                       (uint8_t)(CIP_SERVICE_FORWARD_OPEN | CIP_DONE),
                       (uint8_t)cip_status,
                       ext_status_word_count,
                       ext_status_words,
                      );

        rc = STATUS_OK;
    }


    return rc;
}


/* Forward Close request. */
typedef struct {
    uint8_t secs_per_tick;          /* seconds per tick */
    uint8_t timeout_ticks;          /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint16_t client_connection_serial_number; /* our connection ID/serial number */
    uint16_t client_vendor_id;      /* our unique vendor ID */
    uint32_t client_serial_number;  /* our unique serial number */
    buf_t path;                   /* path to PLC */
} forward_close_s;

/* the minimal Forward Open with no path */
#define CIP_FORWARD_CLOSE_MIN_SIZE   (16)


int process_forward_close(tcp_connection_p connection)
{
    int rc = CIP_OK;
    slice_p pdu = &(client->request);
    slice_p response = &(client->response);
    // buf_t conn_path;
    forward_close_s fc_req = {0};
    uint8_t path_length_words = 0;
    uint8_t fc_cmd = 0;

    info("Processing Forward Close request:");
    buf_dump(request);

    do {
        /* minimum length check */
        if(buf_len(request) < CIP_FORWARD_CLOSE_MIN_SIZE) {
            /* FIXME - send back the right error. */
            make_cip_error(response, (uint8_t)(buf_get_uint8(request) | CIP_DONE), (uint8_t)CIP_ERR_INSUFFICIENT_DATA, false, (uint16_t)0);
            rc = CIP_ERR_INSUFFICIENT_DATA;
            break;
        }

        fc_cmd = buf_get_uint8(request);

        /* get the data. */
        fc_req.secs_per_tick = buf_get_uint8(request);
        fc_req.timeout_ticks = buf_get_uint8(request);
        fc_req.client_connection_serial_number = buf_get_uint16_le(request);
        fc_req.client_vendor_id = buf_get_uint16_le(request);
        fc_req.client_serial_number = buf_get_uint32_le(request);

        path_length_words = *buf_data_ptr(request, buf_cursor(request));

        /* make a buffer for the path.  The +2 is for the word count and pad bytes. */
        fc_req.path = buf_make(buf_data_ptr(request, buf_cursor(request) + 2), path_length_words + 2);
        buf_set_end(&fc_req.path, buf_cap(&fc_req.path));

        /* check the remaining length */
        if(buf_cursor(request) >= buf_len(request)) {
            /* FIXME - send back the right error. */
            info("Forward close request size, %d, is too small!", buf_len(request));
            make_cip_error(response, fc_cmd | CIP_DONE, CIP_ERR_INSUFFICIENT_DATA, false, 0);
            rc = CIP_ERR_INSUFFICIENT_DATA;
            break;
        }

        /*
        * why does Rockwell do this?   The path here is _NOT_ a byte-for-byte copy of the path
        * that was used to open the connection.  This one is padded with a zero byte after the path
        * length.
        */

        /* build the path to match. */
        if((rc = match_path(request, true, client->plc->path, client->plc->path_len)) != CIP_OK) {
            info("path does not match stored path!");
            make_cip_error(response, fc_cmd | CIP_DONE, rc, false, 0);
            break;
        }

        /* Check the values we got. */
        if(client->conn_config.client_connection_serial_number != fc_req.client_connection_serial_number) {
            /* FIXME - send back the right error. */
            info("Forward close connection serial number, %x, did not match the connection serial number originally passed, %x!", fc_req.client_connection_serial_number, client->conn_config.client_connection_serial_number);
            make_cip_error(response, fc_cmd | CIP_DONE, CIP_ERR_INVALID_PARAMETER, false, 0);
            rc = CIP_ERR_INVALID_PARAMETER;
            break;
        }

        if(client->conn_config.client_vendor_id != fc_req.client_vendor_id) {
            /* FIXME - send back the right error. */
            info("Forward close client vendor ID, %x, did not match the client vendor ID originally passed, %x!", fc_req.client_vendor_id, client->conn_config.client_vendor_id);
            make_cip_error(response, fc_cmd | CIP_DONE, CIP_ERR_INVALID_PARAMETER, false, 0);
            rc = CIP_ERR_INVALID_PARAMETER;
            break;
        }

        if(client->conn_config.client_serial_number != fc_req.client_serial_number) {
            /* FIXME - send back the right error. */
            info("Forward close client serial number, %x, did not match the client serial number originally passed, %x!", fc_req.client_serial_number, client->conn_config.client_serial_number);
            make_cip_error(response, fc_cmd | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
            rc = CIP_ERR_UNSUPPORTED;
            break;
        }

        /* now process the FClose and respond. */
        buf_set_uint8(response, CIP_FORWARD_CLOSE[0] | CIP_DONE);
        buf_set_uint8(response, 0); /* padding/reserved. */
        buf_set_uint8(response, CIP_OK); /* no error. */
        buf_set_uint8(response, 0); /* no extra error fields. */

        buf_set_uint16_le(response, client->conn_config.client_connection_serial_number);
        buf_set_uint16_le(response, client->conn_config.client_vendor_id);
        buf_set_uint32_le(response, client->conn_config.client_serial_number);

        /* not sure what these do... */
        buf_set_uint8(response, 0);
        buf_set_uint8(response, 0);
    } while(0);

    return CIP_OK;
}


/*
 * A read request comes in with a symbolic segment first, then zero to three numeric segments.
 */

#define CIP_READ_MIN_SIZE (6)
#define CIP_READ_FRAG_MIN_SIZE (10)

int process_tag_read_request(tcp_connection_p connection)
{
    int rc = CIP_OK;
    slice_p pdu = &(client->request);
    slice_p response = &(client->response);
    uint16_t cip_start_offset = 0;
    uint16_t cip_req_size = 0;
    uint8_t read_cmd = 0;
    uint8_t tag_segment_size = 0;
    uint16_t cip_command_overhead = 0;
    uint16_t element_count = 0;
    uint32_t byte_offset = 0;
    // size_t read_start_offset = 0;
    tag_def_t *tag = NULL;
    size_t tag_data_length = 0;
    size_t total_request_size = 0;
    size_t remaining_size = 0;
    size_t packet_capacity = 0;
    bool need_frag = false;
    size_t amount_to_copy = 0;
    size_t num_elements_to_copy = 0;
    buf_t result;

    info("Processing read request:");
    buf_dump(request);

    /* the cursor is queued up at the service byte. */
    cip_start_offset = buf_get_start(request);
    cip_req_size = buf_len(request);

    do {
        /* check the service before we do anything else. */
        read_cmd = buf_get_uint8(request);

        /* Omron does not support fragmented read. */
        if(client->plc->plc_type == PLC_TYPE_OMRON && read_cmd == CIP_READ_FRAG[0]) {
            info("Omron PLCs do not support fragmented read!");
            make_cip_error(response, read_cmd | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
            rc = CIP_ERR_UNSUPPORTED;
            break;
        }

        cip_command_overhead = (read_cmd == CIP_READ[0] ? CIP_READ_MIN_SIZE : CIP_READ_FRAG_MIN_SIZE);

        if(cip_req_size < cip_command_overhead) {
            info("Insufficient data in the CIP read request!");
            make_cip_error(response, read_cmd | CIP_DONE, CIP_ERR_INSUFFICIENT_DATA, false, 0);
            rc = CIP_ERR_INSUFFICIENT_DATA;
            break;
        }

        tag_segment_size = buf_get_uint8(request);

        /* check that we have at least enough space for the tag name and required data. */
        if((2 * tag_segment_size) + cip_command_overhead > cip_req_size) {
            info("Request does not have enough space for tag name and required fields!");
            make_cip_error(response, read_cmd | CIP_DONE, CIP_ERR_INSUFFICIENT_DATA, false, 0);
            rc = CIP_ERR_INSUFFICIENT_DATA;
            break;
        }

        /* process the tag name and look up the tag.   This eats the tag name data. */
        if((rc = process_tag_segment(client, &tag)) != CIP_OK) {
            make_cip_error(response, read_cmd | CIP_DONE, rc, false, 0);
            break;
        }

        /* get the element count */
        element_count = buf_get_uint16_le(request);

        /* FIXME - this is not required. */
        if(client->plc->plc_type == PLC_TYPE_OMRON) {
            if(element_count != 1) {
                info("Omron PLC requires element count to be 1, found %d!", element_count);
                make_cip_error(response, read_cmd | CIP_DONE, CIP_ERR_INVALID_PARAMETER, false, 0);
                break;
            } else {
                /* all good, now fake it with an element count that is the full tag. */
                element_count = (uint16_t)tag->elem_count;
            }
        }

        if(read_cmd == CIP_READ_FRAG[0]) {
            byte_offset = buf_get_uint32_le(request) + client->access_offset_bytes;
        } else {
            byte_offset = client->access_offset_bytes;
        }

        /* check the offset bounds. */
        tag_data_length = (size_t)(tag->elem_count * tag->elem_size);

        info("tag_data_length = %d", tag_data_length);

        /* get the amount requested. */
        total_request_size = (size_t)(element_count * tag->elem_size);

        info("total_request_size = %d", total_request_size);

        /* check the amount */
        if(total_request_size > tag_data_length) {
            info("request asks for too much data!");
            make_cip_error(response, read_cmd | CIP_DONE, CIP_ERR_EXTENDED, true, CIP_ERR_EX_TOO_LONG);
            rc = CIP_ERR_EX_TOO_LONG;
            break;
        }

        /* check to make sure that the offset passed is within the bounds. */
        if(byte_offset > tag_data_length) {
            info("request offset is past the end of the tag!");
            make_cip_error(response, read_cmd | CIP_DONE, CIP_ERR_EXTENDED, true, CIP_ERR_EX_TOO_LONG);
            rc = CIP_ERR_EX_TOO_LONG;
            break;
        }

        /* do we need to fragment the result? */
        remaining_size = total_request_size - byte_offset;
        packet_capacity = buf_len(response) - 6; /* MAGIC - CIP header plus data type bytes is 6 bytes. */

        info("packet_capacity = %d", packet_capacity);

        if(remaining_size > packet_capacity) {
            need_frag = true;
            amount_to_copy = packet_capacity;
        } else {
            need_frag = false;
            amount_to_copy = remaining_size;
        }

        /* only copy whole elements */
        num_elements_to_copy = amount_to_copy / tag->elem_size;

        info("need_frag = %s", need_frag ? "true" : "false");

        /* start making the response. */
        info("INFO: Writing CIP response header.");

        buf_set_uint8(response, read_cmd | CIP_DONE);
        buf_set_uint8(response, 0); /* padding/reserved. */
        buf_set_uint8(response, (need_frag ? CIP_ERR_FRAG : CIP_OK));
        buf_set_uint8(response, 0); /* no extra error fields. */

        /* copy the data type. */
        info("INFO: writing tag data type.");

        buf_set_uint16_le(response, tag->tag_type);

        info("amount_to_copy = %u", (unsigned int)amount_to_copy);
        info("num_elements_to_copy = %u", (unsigned int)num_elements_to_copy);
        info("copy start location = %u", (unsigned int)buf_cursor(response));
        info("response space = %u", (unsigned int)(buf_len(response) - buf_cursor(response)));

        if(!mutex_lock(&(tag->mutex))) {
            for(size_t i=0; i < (num_elements_to_copy * tag->elem_size) && (byte_offset + i) < tag_data_length; i++) {
                buf_set_uint8(response, tag->data[byte_offset + i]);
            }

            mutex_unlock(&(tag->mutex));
        } else {
            error("ERROR: Unable to lock tag mutex!");
        }

        rc = CIP_OK;
    } while(0);

    return rc;
}



#define CIP_WRITE_MIN_SIZE (6)
#define CIP_WRITE_FRAG_MIN_SIZE (10)

int process_tag_write_request(tcp_connection_p connection)
{
    int rc = CIP_OK;
    slice_p pdu = &(client->request);
    slice_p response = &(client->response);
    uint8_t write_cmd = 0;
    uint8_t tag_segment_size = 0;
    uint32_t byte_offset = 0;
    size_t write_start_offset = 0;
    tag_def_t *tag = NULL;
    size_t tag_data_length = 0;
    size_t total_request_size = 0;
    uint16_t write_data_type = 0;
    uint16_t write_element_count = 0;
    uint16_t cip_request_start_offset = 0;
    buf_t result;

    cip_request_start_offset = buf_cursor(request);

    info("Processing write request:");
    buf_dump(request);

    do {
        write_cmd = buf_get_uint8(request);  /*get the service type. */

        if(buf_len(request) < (write_cmd == CIP_WRITE[0] ? CIP_WRITE_MIN_SIZE : CIP_WRITE_FRAG_MIN_SIZE)) {
            info("Insufficient data in the CIP write request!");
            make_cip_error(response, write_cmd | CIP_DONE, CIP_ERR_INSUFFICIENT_DATA, false, 0);
            rc = CIP_ERR_INSUFFICIENT_DATA;
            break;
        }

        tag_segment_size = *buf_data_ptr(request, buf_cursor(request));

        /* check that we have enough space. */
        if((buf_len(request) + (write_cmd == CIP_WRITE[0] ? 2 : 6) - 2) < (tag_segment_size * 2)) {
            info("Request does not have enough space for element count and byte offset!");
            make_cip_error(response, write_cmd | CIP_DONE, CIP_ERR_INSUFFICIENT_DATA, false, 0);
            rc = CIP_ERR_INSUFFICIENT_DATA;
            break;
        }

        /* process the tag name and look up the tag.   This eats the tag name data. */
        if((rc = process_tag_segment(client, &tag)) != CIP_OK) {
            make_cip_error(response, write_cmd | CIP_DONE, rc, false, 0);
            break;
        }

        /* get the tag data type and compare. */
        write_data_type = buf_get_uint16_le(request);

        /* check that the data types match. */
        if(tag->tag_type != write_data_type) {
            info("tag data type %02x does not match the data type in the write request %02x", tag->tag_type, write_data_type);
            make_cip_error(response, write_cmd | CIP_DONE, CIP_ERR_INVALID_PARAMETER, false, 0);
            rc = CIP_ERR_INVALID_PARAMETER;
            break;
        }

        /* get the number of elements to write. */
        write_element_count = buf_get_uint16_le(request);

        /* check the number of elements */
        if(write_element_count > tag->elem_count) {
            info("request tries to write too many elements!");
            make_cip_error(response, write_cmd | CIP_DONE, CIP_ERR_EXTENDED, true, CIP_ERR_EX_TOO_LONG);
            rc = CIP_ERR_EX_TOO_LONG;
            break;
        }

        if(write_cmd == CIP_WRITE_FRAG[0]) {
            byte_offset = buf_get_uint32_le(request) + client->access_offset_bytes;
        } else {
            byte_offset = client->access_offset_bytes;
        }

        info("byte_offset = %u", byte_offset);

        /* check the offset bounds. */
        tag_data_length = (size_t)(tag->elem_count * tag->elem_size);

        info("tag_data_length = %d", tag_data_length);

        /* get the write amount requested. */
        total_request_size = buf_len(request) - buf_cursor(request);

        info("total_request_size = %d", total_request_size);

        /* check the amount */
        if(total_request_size > tag_data_length - byte_offset) {
            info("request tries to write too much data!");
            make_cip_error(response, write_cmd | CIP_DONE, CIP_ERR_EXTENDED, true, CIP_ERR_EX_TOO_LONG);
            rc = CIP_ERR_EX_TOO_LONG;
            break;
        }

        /* copy the data. */
        if(!mutex_lock(&(tag->mutex))) {
            memcpy(&tag->data[byte_offset], buf_peek_bytes(request), total_request_size);

            mutex_unlock(&(tag->mutex));
        } else {
            error("ERROR: Unable to lock tag mutex!");
        }

        /* start making the response. */
        buf_set_uint8(response, write_cmd | CIP_DONE);
        buf_set_uint8(response, 0); /* padding/reserved. */
        buf_set_uint8(response, CIP_OK);
        buf_set_uint8(response, 0); /* no extra error fields. */

        rc = CIP_OK;
    } while(0);

    return rc;
}


/*
 * we should see:
 *  0x91 <name len> <name bytes> (<numeric segment>){0-3}
 *
 * find the tag name, then check the numeric segments, if any, against the
 * tag dimensions.
 */
status_t process_tag_segment(tcp_connection_p connection, tag_def_t **tag)
{
    int rc = CIP_OK;
    slice_p pdu = &(client->request);
    uint8_t segment_type = 0;
    uint8_t name_len = 0;
    size_t dimensions[3] = { 0, 0, 0};
    size_t dimension_index = 0;
    uint16_t remaining_request_size = 0;
    uint8_t tag_path_words = 0;
    char *tag_name = NULL;

    buf_t tag_path = *request;

    buf_set_cursor(&tag_path, 0);

    /* eat the CIP service type byte */
    buf_get_uint8(&tag_path);

    tag_path_words = buf_get_uint8(request);

    /* set the start past the service byte and the path word count. */
    buf_set_start(&tag_path, buf_cursor_abs(&tag_path));
    buf_set_cursor(&tag_path, 0);

    /* set the end at the end of the tag path bytes. */
    buf_set_end(&tag_path, buf_end(&tag_path) + (2 * tag_path_words));

    rc = process_tag_name(&tag_path, &tag_name, &name_len);
    if(rc != CIP_OK) {
        info("WARN: Error processing tag name!");
        return rc;
    }

    while(*tag) {
        if(buf_match_bytes(request, (uint8_t*)((*tag)->name), name_len)) {
            info("Found tag %s", (*tag)->name);
            break;
        }

        (*tag) = (*tag)->next_tag;
    }

    if(*tag) {
        if(buf_len(&tag_path) > 0) {
            rc = process_tag_dim_index(&tag_path, tag);
            if(rc != CIP_OK) {
                info("WARN: Error processing tag dimension indexes!");
                return rc;
            }
        }
    }

    segment_type = buf_get_uint8(request);

    if(segment_type != CIP_SYMBOLIC_SEGMENT_MARKER)  {
        info("Expected symbolic segment but found %x!", segment_type);
        return CIP_ERR_INVALID_PARAMETER;
    }

    remaining_request_size = buf_len(request) - buf_cursor(request);

    /* get and check the length of the symbolic name part. */
    name_len = buf_get_uint8(request);
    if(name_len >= remaining_request_size) {
        info("Insufficient space in symbolic segment for name.   Needed %d bytes but only had %d bytes!", name_len, remaining_request_size);
        return CIP_ERR_INSUFFICIENT_DATA;
    }

    tag_name = (char *)buf_data_ptr(request, buf_cursor(request));

    /* try to match the tag name. */
    *tag = client->plc->tags;

    while(*tag) {
        if(buf_match_bytes(request, (uint8_t*)((*tag)->name), name_len)) {
            info("Found tag %s", (*tag)->name);
            break;
        }

        (*tag) = (*tag)->next_tag;
    }

    if(*tag) {
        buf_t tag_path = *request;
        uint16_t tag_path_size = 2 * tag_path_words;
        uint16_t remaining_tag_path = 0;
        uint16_t cursor_past_tag_name = 0;

        info("Found tag %.*s", name_len, tag_name);

        /* skip past the name, don't forget the padding */
        cursor_past_tag_name =  1 + /* the CIP read/write service type */
                                1 + /* The length of the tag path in words */
                                1 + /* the 0x91 for the extended symbolic segment type */
                                1 + /* the length count of the tag name in bytes. */
                                name_len + /* the bytes of the tag name */
                                (name_len & 0x01 ? 1 : 0) /* padding byte if name is an odd length */
                                ;

        /* determine whether or not we have something in the tag path after the tag name. */
        info("Tag path words %u.", tag_path_words);
        info("Index past tag name %u.", cursor_past_tag_name);

        if(tag_path_size + 2 < cursor_past_tag_name) {
            /* oops something is wuite wrong with our data here. */
            info("WARN: Malformed tag path?");
            return CIP_ERR_INVALID_PARAMETER;
        }

        remaining_tag_path = tag_path_size +
                             1 + /* CIP service */
                             1 - /* tag path length in words */
                             cursor_past_tag_name /* position after tag symbolic segment */
                             ;

        info("Remaining tag path space %u.", remaining_tag_path);

        if(remaining_tag_path > 0) {
            /* we, hopefully, have numeric segments for the array indexes */

            int num_dims = 0;

            while(1) {
                uint8_t segment_type = buf_get_uint8()
            }
        }

        if()

        buf_set_cursor(&tag_path, 1 + /* the CIP read/write service type */
                                  1 + /* The length of the tag path in words */
                                  1 + /* the 0x91 for the extended symbolic segment type */
                                  1 + /* the length count of the tag name in bytes. */
                                  name_len + /* the bytes of the tag name */
                                  (name_len & 0x01 ? 1 : 0) /* padding byte if name is an odd length */
                       );

        info("Cursor is now %u.", buf_cursor(request));

        /* cap off the length of the numeric segment portion */
        buf_set_len(&numeric_segments, buf_cursor(&numeric_segments) + remaining_tag_path);

        dimension_index = 0;

        info("Numeric segment(s):");
        buf_dump(&numeric_segments);

        while(buf_cursor(&numeric_segments) < buf_len(&numeric_segments)) {
            uint8_t segment_type = 0;

            if(dimension_index >= 3) {
                info("More numeric segments than expected!   Remaining request:");
                buf_dump(&numeric_segments);
                return CIP_ERR_INVALID_PARAMETER;
            }

            segment_type = buf_get_uint8(&numeric_segments);

            switch(segment_type) {
                case 0x28: /* single byte value. */
                    dimensions[dimension_index] = (size_t)buf_get_uint8(&numeric_segments);
                    dimension_index++;
                    break;

                case 0x29: /* two byte value */
                    /* eat the pad byte */
                    buf_get_uint8(&numeric_segments);

                    dimensions[dimension_index] = (size_t)buf_get_uint16_le(&numeric_segments);
                    dimension_index++;
                    break;

                case 0x2A: /* four byte value */
                    /* eat the pad byte */
                    buf_get_uint8(&numeric_segments);

                    dimensions[dimension_index] = (size_t)buf_get_uint32_le(&numeric_segments);
                    dimension_index++;
                    break;

                default:
                    info("Unexpected numeric segment marker %x!", segment_type);
                    return false;
                    break;
            }
        }

        /* calculate the element offset. */
        if(dimension_index > 0) {
            size_t element_offset = 0;

            if(dimension_index != (*tag)->num_dimensions) {
                info("Required %d numeric segments, but only found %d!", (*tag)->num_dimensions, dimension_index);
                return false;
            }

            /* check in bounds. */
            for(size_t i=0; i < dimension_index; i++) {
                if(dimensions[i] >= (*tag)->dimensions[i]) {
                    info("Dimension %d is out of bounds, must be 0 <= %d < %d", (int)i, dimensions[i], (*tag)->dimensions[i]);
                    return false;
                }
            }

            /* calculate the offset. */
            element_offset = (size_t)(dimensions[0] * ((*tag)->dimensions[1] * (*tag)->dimensions[2]) +
                                      dimensions[1] *  (*tag)->dimensions[2] +
                                      dimensions[2]);

            client->access_offset_bytes = (size_t)((*tag)->elem_size * element_offset);
        } else {
            client->access_offset_bytes = 0;
        }
    } else {
        info("Tag %.*s not found!", name_len, (const char *)(tag_name));
        return false;
    }



    return true;
}

int process_tag_name(slice_p tag_path, const char **name, uint8_t *name_len)
{
    int rc = CIP_OK;
    uint8_t segment_type = 0;

    segment_type = buf_get_uint8(tag_path);

    if(segment_type != CIP_SYMBOLIC_SEGMENT_MARKER)  {
        info("Expected symbolic segment but found %x!", segment_type);
        return CIP_ERR_INVALID_PARAMETER;
    }

    /* get and check the length of the symbolic name part. */
    *name_len = buf_get_uint8(tag_path);
    if(*name_len >= buf_len(tag_path) - 2) {
        info("Insufficient space in symbolic segment for name.   Needed %u bytes but only had %u bytes!", *name_len, buf_len(tag_path) - 2);
        return CIP_ERR_INSUFFICIENT_DATA;
    }

    *name = (char *)buf_data_ptr(tag_path, buf_cursor(tag_path));

    info("Found tag symbolic segment %.*s", *name_len, *name);

    return rc;
}


status_t process_tag_dim_segment(slice_p tag_path, tag_def_t *tag)
{
    int rc = CIP_OK;

}



/* match a path.   This is tricky, thanks, Rockwell. */
int match_path(slice_p pdu, bool need_pad, uint8_t *path, uint8_t path_len)
{
    uint8_t input_path_len = 0;

    /* the first byte of the path request is the length byte in 16-bit words */
    input_path_len = buf_get_uint8(request);

    /* check it against the passed path length */
    if((input_path_len * 2) != path_len) {
        info("path is wrong length.   Got %zu but expected %zu!", input_path_len*2, path_len);
        return CIP_ERR_INSUFFICIENT_DATA;
    }

    /* where does the path start? */
    if(need_pad) {
        uint8_t dummy = buf_get_uint8(request);
    }

    if(!buf_match_bytes(request, path, (uint16_t)path_len)) {
        info("Paths do not match!");
        return CIP_ERR_INVALID_PARAMETER;
    } else {
        buf_set_cursor(request, buf_cursor(request) + path_len);
        return CIP_OK;
    }
}



status_t make_cip_error(slice_p encoded_pdu_data, uint8_t cip_cmd, uint8_t cip_err, int8_t num_extended_status_words, uint16_t *extended_status_words)
{
    status_t rc = STATUS_OK;
    cip_response_header_t *response = NULL;

    do {
        /* check size */
        if(slice_get_len(encoded_pdu_data) < (sizeof(*response) + (num_extended_status_words * sizeof(uint16_t)))) {
            /* this one we need to pass back. */
            rc = STATUS_NO_RESOURCE;
            break;
        }

        /* overlay */
        response = (cip_response_header_t *)slice_get_start(encoded_pdu_data);

        response->service = cip_cmd | CIP_DONE;
        response->reserved = 0;
        response->status = cip_err;
        response->ext_status_word_count = num_extended_status_words;

        if(num_extended_status_words > 0) {
            for(uint8_t i=0; i < num_extended_status_words; i++) {
                response->ext_status_words[i] = h2le16(extended_status_words[i]);
            }
        }

        /* cap the response. */
        slice_set_end(encoded_pdu_data, slice_get_start(encoded_pdu_data) + sizeof(*response) + (num_extended_status_words * sizeof(uint16_t)));

        info("CIP error packet:");
        debug_dump_ptr(DEBUG_INFO, slice_get_start(encoded_pdu_data), slice_get_end(encoded_pdu_data));

        rc = STATUS_OK;
    } while(0);

    return rc;
}
