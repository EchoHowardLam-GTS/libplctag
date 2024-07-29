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

#include <stdlib.h>
#include "cpf.h"
#include "eip.h"
#include "buf.h"
#include "tcp_server.h"
#include "utils.h"

#define EIP_REGISTER_SESSION     ((uint16_t)0x0065)
   #define EIP_REGISTER_SESSION_SIZE (4) /* 4 bytes, 2 16-bit words */

#define EIP_UNREGISTER_SESSION   ((uint16_t)0x0066)
#define EIP_UNCONNECTED_SEND     ((uint16_t)0x006F)
#define EIP_CONNECTED_SEND       ((uint16_t)0x0070)

/* supported EIP version */
#define EIP_VERSION     ((uint16_t)1)



typedef struct {
    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
    uint64_t sender_context;
    uint32_t options;
} eip_header_s;


static int register_session(tcp_client_p client, eip_header_s *header);
static int unregister_session(tcp_client_p client, eip_header_s *header);


int eip_dispatch_request(tcp_client_p client)
{
    int rc = TCP_CLIENT_PROCESSED;
    buf_t *request = &(client->request);
    buf_t *response = &(client->response);
    eip_header_s header;

    /* set up the response buffer */
    client->response = client->buffer;

    /* clamp the maximum response to the amount allowed on the connection. */
    buf_set_len(response, client->conn_config.server_to_client_max_packet);

    info("eip_dispatch_request(): got packet:");
    buf_dump(request);

    /* unpack header. */
    buf_set_cursor(request, 0);
    header.command = buf_get_uint16_le(request);
    header.length = buf_get_uint16_le(request);
    header.session_handle = buf_get_uint32_le(request);
    header.status = buf_get_uint32_le(request);
    header.sender_context = buf_get_uint64_le(request);
    header.options = buf_get_uint32_le(request);

    /* sanity checks */
    if(buf_len(request) != (uint16_t)(header.length + (uint16_t)EIP_HEADER_SIZE)) {
        info("Illegal EIP packet.   Length should be %d but is %d!", header.length + EIP_HEADER_SIZE, buf_len(request));
        return TCP_CLIENT_BAD_REQUEST;
    }

    /* set up the response */
    buf_set_len(response, header.length + (uint16_t)EIP_HEADER_SIZE);
    buf_set_cursor(response, (uint16_t)EIP_HEADER_SIZE);

    /* dispatch the request */
    switch(header.command) {
        case EIP_REGISTER_SESSION:
            rc = register_session(client, &header);
            break;

        case EIP_UNREGISTER_SESSION:
            rc = unregister_session(client, &header);
            break;

        case EIP_UNCONNECTED_SEND:
            rc = handle_cpf_unconnected(client);
            break;

        case EIP_CONNECTED_SEND:
            rc = handle_cpf_connected(client);
            break;

        default:
            rc = TCP_CLIENT_UNSUPPORTED;
            break;
    }

    if(rc == EIP_OK) {
        /* build response */
        buf_set_cursor(response, 0);
        buf_set_uint16_le(response, header.command);
        buf_set_uint16_le(response, (uint16_t)(buf_len(response) - (uint16_t)EIP_HEADER_SIZE));
        buf_set_uint32_le(response, client->conn_config.session_handle);
        buf_set_uint32_le(response, (uint32_t)0); /* status == 0 -> no error */
        buf_set_uint64_le(response, client->conn_config.sender_context);
        buf_set_uint32_le(response, header.options);

        /* The payload is already in place. */
        return EIP_OK;
    } else if(rc == TCP_CLIENT_DONE) {
        /* just pass this through, normally not an error. */
        info("Done with connection.");

        return rc;
    } else {
        /* error condition. */
        buf_set_len(response, (uint16_t)EIP_HEADER_SIZE);

        buf_set_uint16_le(response, header.command);
        buf_set_uint16_le(response, (uint16_t)0);  /* no payload. */
        buf_set_uint32_le(response, client->conn_config.session_handle);
        buf_set_uint32_le(response, (uint32_t)rc); /* status */
        buf_set_uint64_le(response, client->conn_config.sender_context);
        buf_set_uint32_le(response, header.options);

        return TCP_CLIENT_BAD_REQUEST;
    }
}


int register_session(tcp_client_p client, eip_header_s *header)
{
    struct {
        uint16_t eip_version;
        uint16_t option_flags;
    } register_request;

    buf_t *request = &(client->request);
    buf_t *response = response;

    /* the cursor is set by the calling routine */
    register_request.eip_version = buf_get_uint16_le(request);
    register_request.option_flags = buf_get_uint16_le(request);

    /* sanity checks.  The command and packet length are checked by now. */

    /* session_handle must be zero. */
    if(header->session_handle != (uint32_t)0) {
        info("Request failed sanity check: request session handle is %u but should be zero.", header->session_handle);

        return EIP_ERR_BAD_REQUEST;
    }

    /* session status must be zero. */
    if(header->status != (uint32_t)0) {
        info("Request failed sanity check: request status is %u but should be zero.", header->status);

        return EIP_ERR_BAD_REQUEST;
    }

    /* session sender context must be zero. */
    if(header->sender_context != (uint64_t)0) {
        info("Request failed sanity check: request sender context should be zero.");

        return EIP_ERR_BAD_REQUEST;
    }

    /* session options must be zero. */
    if(header->options != (uint32_t)0) {
        info("Request failed sanity check: request options is %u but should be zero.", header->options);

        return EIP_ERR_BAD_REQUEST;
    }

    /* EIP version must be 1. */
    if(register_request.eip_version != EIP_VERSION) {
        info("Request failed sanity check: request EIP version is %u but should be %u.", register_request.eip_version, EIP_VERSION);

        return EIP_ERR_BAD_REQUEST;
    }

    /* Session request option flags must be zero. */
    if(register_request.option_flags != (uint16_t)0) {
        info("Request failed sanity check: request option flags field is %u but should be zero.",register_request.option_flags);

        return EIP_ERR_BAD_REQUEST;
    }

    /* all good, generate a session handle. */
    client->conn_config.session_handle = header->session_handle = (uint32_t)rand();

    /* build the response. The calling routine set the cursor. */
    buf_set_uint16_le(response, register_request.eip_version);
    buf_set_uint16_le(response, register_request.option_flags);

    return EIP_OK;
}


int unregister_session(tcp_client_p client, eip_header_s *header)
{
    buf_t *request = &(client->request);
    buf_t *response = response;

    /* the caller set the cursor */
    // buf_set_uint16_le(response);
    if(header->session_handle == client->conn_config.session_handle) {
        return EIP_OK;
    } else {
        info("WARN: session handle does not match session handle in the unregister session request!");
        return EIP_ERR_BAD_REQUEST;
    }
}
