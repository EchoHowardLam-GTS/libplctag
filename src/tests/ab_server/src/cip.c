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


// /* tag commands */
// const uint8_t CIP_MULTI[] = { 0x0A, 0x02, 0x20, 0x02, 0x24, 0x01 };
// const uint8_t CIP_READ[] = { 0x4C };
// const uint8_t CIP_WRITE[] = { 0x4D };
// const uint8_t CIP_RMW[] = { 0x4E, 0x02, 0x20, 0x02, 0x24, 0x01 };
// const uint8_t CIP_READ_FRAG[] = { 0x52 };
// const uint8_t CIP_WRITE_FRAG[] = { 0x53 };


// /* non-tag commands */
// //4b 02 20 67 24 01 07 3d f3 45 43 50 21
// const uint8_t CIP_PCCC_EXECUTE[] = { 0x4B, 0x02, 0x20, 0x67, 0x24, 0x01, 0x07, 0x3d, 0xf3, 0x45, 0x43, 0x50, 0x21 };
// const uint8_t CIP_FORWARD_CLOSE[] = { 0x4E, 0x02, 0x20, 0x06, 0x24, 0x01 };
// const uint8_t CIP_FORWARD_OPEN[] = { 0x54, 0x02, 0x20, 0x06, 0x24, 0x01 };
// const uint8_t CIP_LIST_TAGS[] = { 0x55, 0x02, 0x20, 0x02, 0x24, 0x01 };
// const uint8_t CIP_FORWARD_OPEN_EX[] = { 0x5B, 0x02, 0x20, 0x06, 0x24, 0x01 };


typedef enum {
    CIP_SERVICE_MULTI_REQUEST = 0x0A,
    CIP_SERVICE_PCCC_EXECUTE = 0x4B,
    CIP_SERVICE_READ_TAG = 0x4C,
    CIP_SERVICE_WRITE_TAG = 0x4D,
    CIP_SERVICE_FORWARD_CLOSE = 0x4E, /* DUPE !*/
    CIP_SERVICE_RMW_TAG = 0x4E,  /* DUPE ! */
    CIP_SERVICE_READ_TAG_FRAG = 0x52,
    CIP_SERVICE_WRITE_TAG_FRAG = 0x53,
    CIP_SERVICE_FORWARD_OPEN = 0x54,
    CIP_SERVICE_FORWARD_OPEN_EX = 0x5B,
    CIP_SERVICE_LIST_TAG_ATTRIBS = 0x55,
} cip_service_type_t;

/* path to match. */
// uint8_t LOGIX_CONN_PATH[] = { 0x03, 0x00, 0x00, 0x20, 0x02, 0x24, 0x01 };
// uint8_t MICRO800_CONN_PATH[] = { 0x02, 0x20, 0x02, 0x24, 0x01 };

#define CIP_DONE               ((uint8_t)0x80)

#define CIP_SYMBOLIC_SEGMENT_MARKER ((uint8_t)0x91)

typedef struct {
    uint8_t service_code;   /* why is the operation code _before_ the path? */
    uint8_t path_size;      /* size in 16-bit words of the path */
    slice_t path;           /* store this in a slice to avoid copying */
} cip_header_s;

typedef slice_t epath_t;
typedef epath_t *epath_p;


typedef struct {
    uint8_t service;
    epath_t epath;
    slice_t payload;
} cip_pdu_t;

typedef cip_pdu_t *cip_pdu_p;



static status_t decode_pdu(slice_p request, cip_pdu_p pdu);


static status_t process_forward_open(cip_pdu_p pdu, slice_p response, plc_connection_p connection);
static status_t process_forward_close(slice_p request, slice_p response, plc_connection_p connection);
static status_t process_tag_read_request(slice_p request, slice_p response, plc_connection_p connection);
static status_t process_tag_write_request(slice_p request, slice_p response, plc_connection_p connection);


static status_t process_tag_segment(tcp_connection_p connection, tag_def_t **tag);
static status_t process_tag_dim_segment(slice_p tag_path, tag_def_t *tag);
static bool make_cip_error(slice_p response, uint8_t cip_cmd, uint8_t cip_err, bool extend, uint16_t extended_error);
static int match_path(slice_p request, bool need_pad, uint8_t *path, uint8_t path_len);



status_t cip_dispatch_request(slice_p request, slice_p response, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    cip_pdu_t pdu;

    info("Got CIP request:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        /* decode the PUD into service, epath, and payload chunks */
        rc = decode_pdu(request, &pdu);
        if(rc != STATUS_OK) {
            warn("Got error %s splitting the CIP PDU into chunks!", status_to_str(rc));
            break;
        }

        switch(pdu.service) {
            case CIP_SERVICE_MULTI_REQUEST: rc = STATUS_NOT_SUPPORTED; break;

            case CIP_SERVICE_PCCC_EXECUTE: rc = STATUS_NOT_SUPPORTED; break;

            case CIP_SERVICE_READ_TAG: rc = STATUS_NOT_SUPPORTED; break;

            case CIP_SERVICE_WRITE_TAG: rc = STATUS_NOT_SUPPORTED; break;

            case CIP_SERVICE_FORWARD_CLOSE:     /* DUPE !*/


            // case CIP_SERVICE_RMW_TAG:        /* DUPE ! */

            case CIP_SERVICE_READ_TAG_FRAG: rc = STATUS_NOT_SUPPORTED; break;

            case CIP_SERVICE_WRITE_TAG_FRAG: rc = STATUS_NOT_SUPPORTED; break;

            case CIP_SERVICE_FORWARD_OPEN: rc = process_forward_open(&pdu, response, connection);

            case CIP_SERVICE_FORWARD_OPEN_EX:

            case CIP_SERVICE_LIST_TAG_ATTRIBS: rc = STATUS_NOT_SUPPORTED; break;

            default: rc = STATUS_NOT_RECOGNIZED; break;
        }

        /* handle case where we have unimplemented services */
        if(rc == STATUS_NOT_RECOGNIZED || rc == STATUS_NOT_SUPPORTED) {
            rc = make_cip_error(response, pdu.service, CIP_ERR_UNSUPPORTED, false, 0);
        }

        if(rc == STATUS_)
    } while(0);

    return rc;
}



status_t decode_pdu(slice_p request, cip_pdu_p pdu)
{
    status_t rc = STATUS_OK;

    do {
        uint32_t offset = 0;
        uint8_t epath_word_length = 0;
        uint16_t epath_byte_length = 0;
        slice_t service_header = {0};

        /* get the service byte */
        GET_FIELD(request, u8, &(pdu->service), sizeof(pdu->service));

        /* get the EPATH length in words */
        GET_FIELD(request, u8, &epath_word_length, sizeof(epath_word_length));

        /* calculate the actual length of the epath in bytes. */
        epath_byte_length = 2 * epath_word_length;

        /* split the request slice into pieces */
        rc = slice_split_middle_at_offsets(request, offset, offset + epath_byte_length, NULL, &(pdu->epath), &(pdu->payload));
        if(rc != STATUS_OK) {
            warn("Unable to split the request into EPATH and payload slices, got error %s!", status_to_str(rc));
            break;
        }

        info("Processing CIP PDU with EPATH:");
        debug_dump_buf(DEBUG_INFO, pdu->epath.start, pdu->epath.end);

        info("Processing CIP PDU with payload:");
        debug_dump_buf(DEBUG_INFO, pdu->payload.start, pdu->payload.end);
    } while(0);

    return rc;
}


/* a handy structure to hold all the parameters we need to receive in a Forward Open request. */
typedef struct {
    uint8_t forward_open_service;
    uint8_t secs_per_tick;                  /* seconds per tick */
    uint8_t timeout_ticks;                  /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint32_t server_conn_id;                /* 0, returned by server in reply. */
    uint32_t client_conn_id;                /* sent by client. */
    uint16_t conn_serial_number;            /* client connection ID/serial number */
    uint16_t orig_vendor_id;                /* client unique vendor ID */
    uint32_t orig_serial_number;            /* client unique serial number */
    uint8_t conn_timeout_multiplier;        /* timeout = mult * RPI */
    uint8_t reserved[3];                    /* reserved, set to 0 */
    uint32_t client_to_server_rpi;          /* us to target RPI - Request Packet Interval in microseconds */
    uint32_t client_to_server_conn_params;  /* PDU size from client to us, plus other flags */
    uint32_t server_to_client_rpi;          /* target to us RPI, in microseconds */
    uint32_t server_to_client_conn_params;  /* PDU size from us to client, plus other flags */
    uint8_t transport_class;                /* ALWAYS 0xA3, server transport, class 3, application trigger */
    slice_t connection_path;                /* connection path. */
} forward_open_t;

/* the minimal Forward Open with no path */
#define CIP_FORWARD_OPEN_MIN_SIZE   (42)
#define CIP_FORWARD_OPEN_EX_MIN_SIZE   (46)


status_t process_forward_open(slice_p request, slice_p response, plc_connection_p connection)
{
    status_t rc = STATUS_OK;
    status_t slice_rc = STATUS_OK;
    uint8_t cip_service = 0;
    uint8_t *saved_request_start = NULL;
    uint8_t *saved_response_start = NULL;
    slice_t conn_path = {0};
    forward_open_t forward_open = {0};


    info("Checking Forward Open request:");
    debug_dump_buf(DEBUG_INFO, request->start, request->end);

    do {
        /* save the start of the request for later */
        saved_request_start = request->start;

        slice_rc = slice_unpack(request, "&u1", &forward_open.forward_open_service);

        assert_error((slice_rc == STATUS_OK), STATUS_BAD_INPUT, "Unable to unpack request slice!");

        /* some of the services need the service */
        request->start = saved_request_start;

        /* store the response start for later. */
        saved_response_start = response->start;

        /* unpack the first common part. */
        slice_rc = slice_unpack(request, "u1,u1,u4,u4,u2,u2,u4,u1,00,00,00,u4",
                                          &forward_open.secs_per_tick,
                                          &forward_open.timeout_ticks,
                                          &forward_open.server_conn_id, /* must be zero */
                                          &forward_open.client_conn_id,
                                          &forward_open.conn_serial_number,
                                          &forward_open.orig_vendor_id,
                                          &forward_open.orig_serial_number,
                                          &forward_open.conn_timeout_multiplier,
                                          &forward_open.client_to_server_rpi
                                );

        assert_error((slice_rc == STATUS_OK), STATUS_BAD_INPUT, "Unable to unpack request slice!");

        if(forward_open.forward_open_service == CIP_SERVICE_FORWARD_OPEN) {
            uint16_t c2s_params = 0;
            uint16_t s2c_params = 0;

            /* now unpack the client params which are service-specific */
            slice_rc = slice_unpack(request, "u2", &c2s_params);

            forward_open.client_to_server_conn_params = (uint32_t)c2s_params;
        } else {
            slice_rc = slice_unpack(request, "u4", &forward_open.client_to_server_conn_params);
        }

        assert_error((slice_rc == STATUS_OK), STATUS_BAD_INPUT, "Unable to unpack request slice!");

        slice_rc = slice_unpack(request, "u4", &forward_open.server_to_client_rpi);

        assert_error((slice_rc == STATUS_OK), STATUS_BAD_INPUT, "Unable to unpack request slice!");

        if(forward_open.forward_open_service == CIP_SERVICE_FORWARD_OPEN) {
            uint16_t s2c_params = 0;

            /* now unpack the client params which are service-specific */
            slice_rc = slice_unpack(request, "u2", &s2c_params);

            forward_open.server_to_client_conn_params = (uint32_t)s2c_params;
        } else {
            slice_rc = slice_unpack(request, "u4", &forward_open.server_to_client_conn_params);
        }

        assert_error((slice_rc == STATUS_OK), STATUS_BAD_INPUT, "Unable to unpack request slice!");

        /* now get the rest */
        slice_rc = slice_unpack(request, "u8,e",
                                          &forward_open.transport_class,
                                          &forward_open.connection_path
                               );
        assert_error((slice_rc == STATUS_OK), STATUS_BAD_INPUT, "Unable to unpack request slice!");

        /* check to see how many refusals we should do. */
        if(connection->cip_connection.reject_fo_count > 0) {
            connection->cip_connection.reject_fo_count--;

            info("Forward open request being bounced for debugging. %d to go.", connection->cip_connection.reject_fo_count);

            make_cip_error(response,
                           (uint8_t)(buf_get_uint8(request) | CIP_DONE),
                           (uint8_t)CIP_ERR_FLAG,
                           true,
                           (uint16_t)CIP_ERR_EX_DUPLICATE_CONN
                          );

            rc = STATUS_OK;

            break;
        }

        /* all good if we got here. Save some values to the persistent connection state. */
        connection->cip_connection.client_connection_id = forward_open.client_conn_id;
        connection->cip_connection.client_connection_serial_number = forward_open.conn_serial_number;
        connection->cip_connection.client_vendor_id = forward_open.orig_vendor_id;
        connection->cip_connection.client_serial_number = forward_open.orig_serial_number;
        connection->cip_connection.client_to_server_rpi = forward_open.client_to_server_rpi;
        connection->cip_connection.server_to_client_rpi = forward_open.server_to_client_rpi;
        connection->cip_connection.server_connection_id = (uint32_t)rand();
        connection->cip_connection.server_connection_seq = (uint16_t)rand();

        /* calculate the allowed packet sizes. */
        connection->cip_connection.client_to_server_max_packet = (forward_open.client_to_server_conn_params &
                                ((forward_open.forward_open_service == CIP_SERVICE_FORWARD_OPEN) ? 0x1FF : 0x0FFF)) + 64; /* MAGIC */
        connection->cip_connection.server_to_client_max_packet = forward_open.server_to_client_conn_params &
                                ((forward_open.forward_open_service == CIP_SERVICE_FORWARD_OPEN) ? 0x1FF : 0x0FFF);

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
    slice_p request = &(client->request);
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
    slice_p request = &(client->request);
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
    slice_p request = &(client->request);
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
    slice_p request = &(client->request);
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
int match_path(slice_p request, bool need_pad, uint8_t *path, uint8_t path_len)
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



bool make_cip_error(slice_p response, uint8_t cip_cmd, uint8_t cip_err, bool extend, uint16_t extended_error)
{
    /* cursor is set higher up in the call chain */

    buf_set_uint8(response, cip_cmd | CIP_DONE);
    buf_set_uint8(response, 0); /* reserved, must be zero. */
    buf_set_uint8(response, cip_err);

    if(extend) {
        buf_set_uint8(response, 2); /* two bytes of extended status. */
        buf_set_uint16_le(response, extended_error);
    } else {
        buf_set_uint8(response, 0); /* no additional bytes of sub-error. */
    }

    return true;
}
