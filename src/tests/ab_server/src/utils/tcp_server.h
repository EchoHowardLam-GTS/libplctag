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

#pragma once

#include <signal.h>
#include "slice.h"
#include "socket.h"
#include "status.h"




struct app_data_t {

};

typedef struct app_data_t app_data_t;
typedef struct app_data_t *app_data_p;


struct app_connection_data_t {

};

typedef struct app_connection_data_t app_connection_data_t;
typedef struct app_connection_data_t *app_connection_data_p;



typedef bool (*program_terminating_func)(app_data_p app_data);
typedef void (*terminate_program_func)(app_data_p app_data);
typedef status_t (*init_app_connection_data_func)(app_connection_data_p app_connection_data, app_data_p app_data);
typedef status_t (*clean_up_app_connection_data_func)(app_connection_data_p app_connection_data, app_data_p app_data);

typedef status_t (*process_request_func)(slice_p pdu, app_connection_data_p app_connection_data, app_data_p app_data);


typedef struct tcp_server_config_t {
    const char *host;
    const char *port;

    uint32_t buffer_size;
    uint32_t app_connection_data_size;

    app_data_p app_data;

    program_terminating_func program_terminating;
    terminate_program_func terminate_program;
    init_app_connection_data_func init_app_connection_data;
    clean_up_app_connection_data_func clean_up_app_connection_data;
    process_request_func process_request;
} tcp_server_config_t;

typedef struct tcp_server_config_t *tcp_server_config_p;


extern void tcp_server_run(tcp_server_config_p config);
