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


#ifndef NET_EVENT_H
#define NET_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buf.h"

typedef enum {
    NET_EVENT_STATUS_OK,
    NET_EVENT_STATUS_NULL_PTR,
    NET_EVENT_STATUS_ACCEPT_ERROR,
    NET_EVENT_STATUS_READ_ERROR,
    NET_EVENT_STATUS_WRITE_ERROR,
    NET_EVENT_STATUS_NOT_SUPPORTED,
    NET_EVENT_STATUS_ERROR,
} status_t;

typedef enum {
    NET_EVENT_SOCKET_TYPE_UDP,
    NET_EVENT_SOCKET_TYPE_TCP_LISTENER,
    NET_EVENT_SOCKET_TYPE_TCP_CLIENT,
} net_event_socket_type_t;

typedef enum {
    NET_EVENT_CALLBACK_RESULT_CLEAR,
    NET_EVENT_CALLBACK_RESULT_RESET,
} net_event_callback_result_t;


struct net_event_socket_t;
struct net_event_manager_t;

struct net_event_socket_cb_config_t {
    net_event_callback_result_t (*on_accepted_cb)(struct net_event_socket_t *socket, struct net_event_socket_t *client_socket, status_t status, void *app_data, void *socket_data);
    net_event_callback_result_t (*on_close_cb)(struct net_event_socket_t *socket, status_t status, void *app_data, void *socket_data);
    net_event_callback_result_t (*on_received_cb)(struct net_event_socket_t *socket, const char *sender_ip, uint16_t sender_port, struct buf_t *buffer, status_t status, void *app_data, void *socket_data);
    net_event_callback_result_t (*on_sent_cb)(struct net_event_socket_t *socket, struct buf_t *buffer, status_t status, void *app_data, void *socket_data);
    net_event_callback_result_t (*on_tick_cb)(struct net_event_socket_t *socket, status_t status, void *app_data, void *socket_data);
    net_event_callback_result_t (*on_wake_cb)(struct net_event_socket_t *socket, status_t status, void *app_data, void *socket_data);
};

struct net_event_manager_cb_config_t {
    net_event_callback_result_t (*on_dispose_cb)(struct net_event_manager_t *event_manager, void *app_data);
    net_event_callback_result_t (*on_start_cb)(struct net_event_manager_t *event_manager, void *app_data);
    net_event_callback_result_t (*on_stop_cb)(struct net_event_manager_t *event_manager, void *app_data);
    net_event_callback_result_t (*on_tick_cb)(struct net_event_manager_t *event_manager, void *app_data);
    net_event_callback_result_t (*on_wake_cb)(struct net_event_manager_t *event_manager, void *app_data);
};

extern struct net_event_manager_t *net_event_manager_create(uint32_t tick_period_ms, void *app_data, struct net_event_manager_cb_config_t *cb_config);
extern status_t net_event_manager_dispose(struct net_event_manager_t *event_manager);
extern status_t net_event_manager_start(struct net_event_manager_t *event_manager);
extern status_t net_event_manager_stop(struct net_event_manager_t *event_manager);
extern status_t net_event_manager_wake(struct net_event_manager_t *event_manager);


extern struct net_event_socket_t *net_event_socket_open(struct net_event_manager_t *net_event_manager, net_event_socket_type_t socket_type, const char *address, uint16_t port, void *sock_data, struct net_event_socket_cb_config_t *cb_config);
extern status_t net_event_socket_close(struct net_event_socket_t *socket);
extern status_t net_event_socket_set_app_data(struct net_event_socket_t *socket, void *sock_data);
extern status_t net_event_socket_set_cb_config(struct net_event_socket_t *socket, struct net_event_socket_cb_config_t *cb_config);


#endif
