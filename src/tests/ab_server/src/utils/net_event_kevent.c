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

#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include "net_event.h"

struct net_event_socket_t
{
    struct net_event_socket_t *next;
    int fd;
    net_event_socket_type_t type;
    struct net_event_socket_cb_config_t cb_config;
    void *sock_data;
    struct buf_t *buffer;
    const char *remote_addr;
    uint16_t remote_port;
    struct net_event_manager_t *event_manager;
};

typedef struct net_event_manager_t
{
    int kq;
    // int wake_pipe[2]; // Pipe for waking up the event loop
    pthread_t event_thread;
    bool running;
    uint32_t tick_period_ms;
    struct net_event_manager_cb_config_t cb_config;
    void *app_data;
    struct net_event_socket_t *sockets;
} net_event_manager_t;

static int make_fd_non_blocking(int fd);
static void *event_loop_thread(void *arg);

struct net_event_manager_t *net_event_manager_create(uint32_t tick_period_ms, void *app_data, struct net_event_manager_cb_config_t *cb_config)
{
    net_event_manager_t *event_mgr = calloc(1, sizeof(net_event_manager_t));
    if (!event_mgr)
    {
        return NULL;
    }

    event_mgr->tick_period_ms = tick_period_ms;

    event_mgr->app_data = app_data;
    if (cb_config)
    {
        event_mgr->cb_config = *cb_config;
    }

    // Create kqueue
    event_mgr->kq = kqueue();
    if (event_mgr->kq < 0)
    {
        free(event_mgr);
        return NULL;
    }

    /* set up tick timer */
    struct kevent timer_event;
    EV_SET(&timer_event, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, event_mgr->tick_period_ms, NULL);
    kevent(event_mgr->kq, &timer_event, 1, NULL, 0, NULL);

    /* set up wake event */

    // // Create pipe for waking up the event loop
    // if (pipe(event_mgr->wake_pipe) < 0) {
    //     close(event_mgr->kq);
    //     free(event_mgr);
    //     return NULL;
    // }

    // // Make the pipe non-blocking
    // make_fd_non_blocking(event_mgr->wake_pipe[0]);
    // make_fd_non_blocking(event_mgr->wake_pipe[1]);

    // struct net_event_manager_t *event_manager = (struct net_event_manager_t *)event_mgr;
    return event_mgr;
}

// Modify net_event_manager_dispose to close the wake pipe
status_t net_event_manager_dispose(struct net_event_manager_t *event_manager)
{
    net_event_manager_t *event_mgr = (net_event_manager_t *)event_manager;

    // FIXME - call stop
    net_event_manager_stop(event_mgr);

    /* close all sockets */
    while(event_mgr->sockets) {
        net_event_socket_close(event_mgr->sockets);
    }

    close(event_mgr->kq);

    // if (pthread_join(event_mgr->event_thread, NULL) != 0) {
    //     // FIXME - use real status.
    //     return NET_EVENT_STATUS_NOT_SUPPORTED;
    // }

    if (event_mgr->cb_config.on_dispose_cb)
    {
        event_mgr->cb_config.on_dispose_cb(event_manager, event_mgr->app_data);
    }

    free(event_mgr);

    return NET_EVENT_STATUS_OK;
}

// Modify net_event_manager_start to register the wake pipe with the kqueue
status_t net_event_manager_start(struct net_event_manager_t *event_mgr)
{
    event_mgr->running = true;

    /* set up tick timer */
    struct kevent timer_event;
    EV_SET(&timer_event, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, event_mgr->tick_period_ms, NULL);
    kevent(event_mgr->kq, &timer_event, 1, NULL, 0, NULL);

    // Create and start the event loop thread
    if (pthread_create(&event_mgr->event_thread, NULL, event_loop_thread, event_mgr) != 0)
    {
        return NET_EVENT_STATUS_NOT_SUPPORTED;
    }

    if (event_mgr->cb_config.on_start_cb)
    {
        event_mgr->cb_config.on_start_cb(event_mgr, event_mgr->app_data);
    }

    return NET_EVENT_STATUS_OK;
}

status_t net_event_manager_stop(struct net_event_manager_t *event_manager)
{
    struct kevent ev;

    event_manager->running = false;

    net_event_manager_wake(event_manager);

    if (pthread_join(event_manager->event_thread, NULL) != 0)
    {
        // FIXME - use real status.
        return NET_EVENT_STATUS_NOT_SUPPORTED;
    }

    event_manager->event_thread = NULL;

    /* remove the tick timer */
    EV_SET(&ev, 1, EVFILT_TIMER, EV_DELETE, 0, event_manager->tick_period_ms, NULL);
    kevent(event_manager->kq, &ev, 1, NULL, 0, NULL);

    if (event_manager->cb_config.on_stop_cb)
    {
        event_manager->cb_config.on_stop_cb(event_manager, event_manager->app_data);
    }

    return NET_EVENT_STATUS_OK;
}


status_t net_event_manager_wake(struct net_event_manager_t *event_manager)
{

    struct kevent wake_event;
    EV_SET(&wake_event, 1, EVFILT_USER, EV_ADD | EV_CLEAR, NOTE_TRIGGER, 0, NULL);
    kevent(event_manager->kq, &wake_event, 1, NULL, 0, NULL);

    /* call the wake callback */
    if (event_manager->cb_config.on_wake_cb)
    {
        event_manager->cb_config.on_wake_cb(event_manager, event_manager->app_data);
    }

    return NET_EVENT_STATUS_OK;
}

/****************************************************
 *    SOCKET FUNCTIONS                              *
 ****************************************************/

status_t net_event_socket_close(struct net_event_socket_t *socket)
{
    /* remove from the network manager's list. */
    struct net_event_manager_t *event_mgr = socket->event_manager;
    struct kevent ev;
    struct net_event_socket_t **sock_list_walker = &event_mgr->sockets;

    while (*sock_list_walker && *sock_list_walker != socket)
    {
        sock_list_walker = &((*sock_list_walker)->next);
    }

    if (*sock_list_walker && *sock_list_walker == socket)
    {
        *sock_list_walker = socket->next;
    }

    /* remove the event */
    EV_SET(&ev, socket->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(event_mgr->kq, &ev, 1, NULL, 0, NULL);

    EV_SET(&ev, socket->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(event_mgr->kq, &ev, 1, NULL, 0, NULL);

    /* close the socket */
    close(socket->fd);

    if (socket->cb_config.on_close_cb)
    {
        socket->cb_config.on_close_cb(socket, NET_EVENT_STATUS_OK, NULL, socket->sock_data);
    }

    free(socket);

    return NET_EVENT_STATUS_OK;
}

status_t net_event_socket_set_app_data(struct net_event_socket_t *socket, void *sock_data)
{
    socket->sock_data = sock_data;
    return NET_EVENT_STATUS_OK;
}

status_t net_event_socket_set_cb_config(struct net_event_socket_t *socket, struct net_event_socket_cb_config_t *cb_config)
{
    if (cb_config)
    {
        socket->cb_config = *cb_config;
    }
    return NET_EVENT_STATUS_OK;
}

status_t net_event_socket_start_receive(struct net_event_socket_t *socket, struct buf_t *buffer)
{
    struct kevent ev;

    socket->buffer = buffer;

    EV_SET(&ev, socket->fd, EVFILT_READ, EV_ADD, 0, 0, socket);
    if (kevent(((net_event_manager_t *)socket->event_manager)->kq, &ev, 1, NULL, 0, NULL) < 0)
    {
        return NET_EVENT_STATUS_ERROR;
    }

    return NET_EVENT_STATUS_OK;
}

status_t net_event_socket_start_send(struct net_event_socket_t *socket, struct buf_t *buffer, const char *remote_addr, uint16_t remote_port)
{
    struct kevent ev;

    socket->buffer = buffer;

    socket->remote_addr = remote_addr;
    socket->remote_port = remote_port;

    EV_SET(&ev, socket->fd, EVFILT_WRITE, EV_ADD, 0, 0, socket);
    if (kevent(((net_event_manager_t *)socket->event_manager)->kq, &ev, 1, NULL, 0, NULL) < 0)
    {
        return NET_EVENT_STATUS_ERROR;
    }

    return NET_EVENT_STATUS_OK;
}

status_t net_event_socket_start_accept(struct net_event_socket_t *socket)
{
    struct kevent ev;
    EV_SET(&ev, socket->fd, EVFILT_READ, EV_ADD, 0, 0, socket);
    if (kevent(((net_event_manager_t *)socket->event_manager)->kq, &ev, 1, NULL, 0, NULL) < 0)
    {
        return NET_EVENT_STATUS_ERROR;
    }

    return NET_EVENT_STATUS_OK;
}

status_t net_event_socket_wake(struct net_event_socket_t *socket)
{
    if (socket->cb_config.on_wake_cb)
    {
        socket->cb_config.on_wake_cb(socket, NET_EVENT_STATUS_OK, NULL, socket->sock_data);
    }

    return NET_EVENT_STATUS_OK;
}

struct net_event_socket_t *net_event_socket_open(struct net_event_manager_t *net_event_manager, net_event_socket_type_t socket_type, const char *address, uint16_t port, void *sock_data, struct net_event_socket_cb_config_t *cb_config)
{
    struct net_event_socket_t *new_socket = calloc(1, sizeof(struct net_event_socket_t));
    if (!socket)
    {
        return NULL;
    }

    new_socket->event_manager = net_event_manager;
    new_socket->sock_data = sock_data;
    if (cb_config)
    {
        new_socket->cb_config = *cb_config;
    }

    // Creating socket
    new_socket->fd = socket(AF_INET, ((socket_type == NET_EVENT_SOCKET_TYPE_TCP_CLIENT) || (socket_type == NET_EVENT_SOCKET_TYPE_TCP_LISTENER)) ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (new_socket->fd < 0)
    {
        free(new_socket);
        return NULL;
    }

    /* make the socket non-blocking */
    // FIXME - handle errors
    make_socket_non_blocking(new_socket->fd);

    // Setting up address
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(address);
    addr.sin_port = htons(port);

    /* link the new socket into the event manager's list */
    new_socket->next = net_event_manager->sockets;
    net_event_manager->sockets = new_socket;

    if (socket_type == NET_EVENT_SOCKET_TYPE_TCP_CLIENT)
    {
        // FIXME - handle errors!
        connect(new_socket->fd, &addr, sizeof(addr));
    }
    else
    {
        /* handle UDP and listening sockets */
        if (bind(new_socket->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(new_socket->fd);
            free(new_socket);
            return NULL;
        }

        if (socket_type == NET_EVENT_SOCKET_TYPE_TCP_LISTENER)
        {
            // FIXME - handle errors
            listen(new_socket->fd, 5);
        }
    }

    return (struct net_event_socket_t *)new_socket;
}

/* SUPPORT FUNCTIONS */

// Make a file descriptor non-blocking
static int make_fd_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

#define MAX_EVENTS (32)

static void *event_loop_thread(void *arg)
{
    struct net_event_manager_t *manager = (struct net_event_manager_t *)arg;
    struct kevent events[MAX_EVENTS];
    struct net_event_socket_t *current_socket;

    while (manager->running)
    {
        // Wait for events to occur
        int nev = kevent(manager->kq, NULL, 0, events, MAX_EVENTS, NULL);
        // FIXME - check errors

        // Iterate through triggered events
        for (int i = 0; i < nev; i++)
        {
            if (events[i].filter == EVFILT_USER && events[i].fflags & NOTE_TRIGGER)
            {
                // Wake event for the event manager
                if (manager->cb_config.on_wake_cb)
                {
                    manager->cb_config.on_wake_cb(manager, manager->app_data);
                }
                continue;
            }

            struct net_event_socket_t *socket = (struct net_event_socket_t *)events[i].udata;

            // TCP Listener: Accept new connections
            if (socket->type == NET_EVENT_SOCKET_TYPE_TCP_LISTENER && events[i].filter == EVFILT_READ)
            {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(socket->fd, (struct sockaddr *)&client_addr, &addr_len);

                if (client_fd >= 0)
                {
                    set_nonblocking(client_fd); // Make new client socket non-blocking
                    struct net_event_socket_t *client_socket = (struct net_event_socket_t *)malloc(sizeof(struct net_event_socket_t));
                    client_socket->fd = client_fd;
                    client_socket->type = NET_EVENT_SOCKET_TYPE_TCP_CLIENT;
                    client_socket->event_manager = manager;
                    client_socket->sock_data = socket->sock_data;
                    client_socket->cb_config = socket->cb_config;

                    // Add the new client socket to the event loop for reading
                    struct kevent client_event;
                    EV_SET(&client_event, client_fd, EVFILT_READ, EV_ADD, 0, 0, client_socket);
                    kevent(manager->kq, &client_event, 1, NULL, 0, NULL);

                    // Call the accepted callback
                    if (socket->cb_config.on_accepted_cb)
                    {
                        socket->cb_config.on_accepted_cb(socket, client_socket, NET_EVENT_STATUS_OK, manager->app_data, socket->sock_data);
                    }
                }
                else
                {
                    // Handle accept error
                    if (socket->cb_config.on_accepted_cb)
                    {
                        socket->cb_config.on_accepted_cb(socket, NULL, NET_EVENT_STATUS_ACCEPT_ERROR, manager->app_data, socket->sock_data);
                    }
                }
            }

            // TCP Client: Data received from a client
            else if (socket->type == NET_EVENT_SOCKET_TYPE_TCP_CLIENT && events[i].filter == EVFILT_READ)
            {
                char buffer[1024]; // FIXME - move this to the passed buffer.
                ssize_t bytes_received = recv(socket->fd, buffer, sizeof(buffer), 0);

                if (bytes_received > 0)
                {
                    struct buf_t buf = {(uint8_t *)buffer, (size_t)bytes_received};

                    // Call the received callback
                    if (socket->cb_config.on_received_cb)
                    {
                        socket->cb_config.on_received_cb(socket, NULL, 0, &buf, NET_EVENT_STATUS_OK, manager->app_data, socket->sock_data);
                    }
                }
                else if (bytes_received == 0)
                {
                    // Connection closed by the client
                    close(socket->fd);
                    if (socket->cb_config.on_close_cb)
                    {
                        socket->cb_config.on_close_cb(socket, NET_EVENT_STATUS_OK, manager->app_data, socket->sock_data);
                    }
                }
                else
                {
                    // Error in receiving data
                    if (socket->cb_config.on_received_cb)
                    {
                        socket->cb_config.on_received_cb(socket, NULL, 0, NULL, NET_EVENT_STATUS_READ_ERROR, manager->app_data, socket->sock_data);
                    }
                }
            }

            // Proactor-style writing for TCP Client: Write data fully, then call the callback
            else if (socket->type == NET_EVENT_SOCKET_TYPE_TCP_CLIENT && events[i].filter == EVFILT_WRITE)
            {
                if (socket->buffer && socket->buffer->len > 0)
                {
                    ssize_t bytes_sent = send(socket->fd, socket->buffer->data, socket->buffer->len, 0);

                    if (bytes_sent > 0)
                    {
                        // Move buffer forward
                        socket->buffer->data += bytes_sent;
                        socket->buffer->len -= bytes_sent;

                        // If all data is sent, call the write completion callback
                        if (socket->buffer->len == 0)
                        {
                            if (socket->cb_config.on_sent_cb)
                            {
                                socket->cb_config.on_sent_cb(socket, NET_EVENT_STATUS_OK, socket->buffer, manager->app_data, socket->sock_data);
                            }
                        }
                        else
                        {
                            /* still data to send */
                            net_event_socket_start_send(socket, socket->buffer, socket->remote_addr, socket->remote_port);
                        }
                    }
                    else
                    {
                        // Handle write error
                        if (socket->cb_config.on_sent_cb)
                        {
                            socket->cb_config.on_sent_cb(socket, NET_EVENT_STATUS_WRITE_ERROR, socket->buffer, manager->app_data, socket->sock_data);
                        }
                    }
                }
            }

            // UDP Socket: Data received on a UDP socket
            else if (socket->type == NET_EVENT_SOCKET_TYPE_UDP && events[i].filter == EVFILT_READ)
            {
                struct sockaddr_in sender_addr;
                socklen_t addr_len = sizeof(sender_addr);
                char buffer[1024];

                ssize_t bytes_received = recvfrom(socket->fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender_addr, &addr_len);
                if (bytes_received > 0)
                {
                    struct buf_t buf = {(uint8_t *)buffer, (size_t)bytes_received};
                    char sender_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
                    uint16_t sender_port = ntohs(sender_addr.sin_port);

                    // Call the received callback
                    if (socket->cb_config.on_received_cb)
                    {
                        socket->cb_config.on_received_cb(socket, sender_ip, sender_port, &buf, NET_EVENT_STATUS_OK, manager->app_data, socket->sock_data);
                    }
                }
                else
                {
                    // Error in receiving data
                    if (socket->cb_config.on_received_cb)
                    {
                        socket->cb_config.on_received_cb(socket, NULL, 0, NULL, NET_EVENT_STATUS_READ_ERROR, manager->app_data, socket->sock_data);
                    }
                }
            }

            // UDP Socket: Proactor-style writing for UDP
            else if (socket->type == NET_EVENT_SOCKET_TYPE_UDP && events[i].filter == EVFILT_WRITE)
            {
                if (socket->buffer && socket->buffer->len > 0)
                {
                    struct sockaddr_in dest_addr;
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(socket->remote_port);
                    inet_pton(AF_INET, socket->remote_addr, &dest_addr.sin_addr);

                    ssize_t bytes_sent = sendto(socket->fd, socket->buffer->data, socket->buffer->len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

                    if (bytes_sent > 0)
                    {
                        socket->buffer->data += bytes_sent;
                        socket->buffer->len -= bytes_sent;

                        if (socket->buffer->len == 0)
                        {
                            // All data sent, call the callback
                            if (socket->cb_config.on_sent_cb)
                            {
                                socket->cb_config.on_sent_cb(socket, NET_EVENT_STATUS_OK, socket->buffer, manager->app_data, socket->sock_data);
                            }
                        }
                        else
                        {
                            /* still data to send */
                            net_event_socket_start_send(socket, socket->buffer, socket->remote_addr, socket->remote_port);
                        }
                    }
                    else
                    {
                        // Handle send error
                        if (socket->cb_config.on_sent_cb)
                        {
                            socket->cb_config.on_sent_cb(socket, NET_EVENT_STATUS_WRITE_ERROR, socket->buffer, manager->app_data, socket->sock_data);
                        }
                    }
                }
            }
        }

        // Tick event for the manager
        if (manager->cb_config.on_tick_cb)
        {
            manager->cb_config.on_tick_cb(manager, manager->app_data);
        }

        // Tick event for each socket
        current_socket = manager->sockets;
        while (current_socket)
        {
            if (current_socket->cb_config.on_tick_cb)
            {
                current_socket->cb_config.on_tick_cb(current_socket, NET_EVENT_STATUS_OK, manager->app_data, current_socket->sock_data);
            }
            current_socket = current_socket->next;
        }
    }
    return NULL;
}

// // Event loop that handles both socket events and the wake pipe
// static void *event_loop_thread_old(void *arg)
// {
//     net_event_manager_t *event_mgr = (net_event_manager_t *)arg;
//     struct kevent events[32];

//     while (event_mgr->running) {
//         struct timespec timeout = {event_mgr->tick_period_ms / 1000, (event_mgr->tick_period_ms % 1000) * 1000000};
//         int nev = kevent(event_mgr->kq, NULL, 0, events, 32, &timeout);

//         if (nev < 0) {
//             perror("kevent error");
//             break;
//         }

//         for (int i = 0; i < nev; i++) {
//             struct kevent *event = &events[i];
//             struct net_event_socket_t *socket = (struct net_event_socket_t *)events[i].udata;

//             if (socket->type == NET_EVENT_SOCKET_TYPE_TCP_LISTENER) {
//                 struct sockaddr_in client_addr;
//                 socklen_t addr_len = sizeof(client_addr);
//                 int client_fd = accept(socket->fd, (struct sockaddr *)&client_addr, &addr_len);

//                 if (client_fd >= 0) {
//                     set_nonblocking(client_fd);
//                     struct net_event_socket_t *client_socket = (struct net_event_socket_t *)malloc(sizeof(struct net_event_socket_t));
//                     client_socket->fd = client_fd;
//                     client_socket->type = NET_EVENT_SOCKET_TYPE_TCP_CLIENT;
//                     client_socket->event_manager = event_mgr;
//                     client_socket->sock_data = socket->sock_data;
//                     client_socket->cb_config = socket->cb_config;

//                     EV_SET(&events[i], client_fd, EVFILT_READ, EV_ADD, 0, 0, client_socket);
//                     kevent(event_mgr->kq, &events[i], 1, NULL, 0, NULL);

//                     if (socket->cb_config.on_accepted_cb) {
//                         socket->cb_config.on_accepted_cb(socket, client_socket, NET_EVENT_STATUS_OK, event_mgr->app_data, socket->sock_data);
//                     }
//                 } else {
//                     // Handle accept error
//                     if (socket->cb_config.on_accepted_cb) {
//                         socket->cb_config.on_accepted_cb(socket, NULL, NET_EVENT_STATUS_ACCEPT_ERROR, event_mgr->app_data, socket->sock_data);
//                     }
//                 }
//             } else {  /* not listening sockets */
//                 if (event->ident == event_mgr->wake_pipe[0]) {
//                     // Handle wake-up event from the pipe
//                     char buffer[16];  // MAGIC

//                     // FIXME - Multiple wake calls could have happened.
//                     read(event_mgr->wake_pipe[0], buffer, sizeof(buffer)); // Clear the pipe

//                     if (event_mgr->cb_config.on_wake_cb) {
//                         event_mgr->cb_config.on_wake_cb((struct net_event_manager_t *)event_mgr, event_mgr->app_data);
//                     }
//                 } else if (event->flags & EV_ERROR) {
//                     // Handle error in the event
//                     continue;
//                 } else {
//                     // Handle socket events (e.g., read, write)
//                     if (event->filter == EVFILT_READ) {
//                         // FIXME - actually do the read
//                         if (socket->cb_config.on_received_cb) {
//                             socket->cb_config.on_received_cb(socket, NET_EVENT_STATUS_OK, event->data, socket->sock_data);
//                         }
//                     } else if (event->filter == EVFILT_WRITE) {
//                         // FIXME - do the write.
//                         if (socket->cb_config.on_sent_cb) {
//                             socket->cb_config.on_sent_cb(socket, NET_EVENT_STATUS_OK, event->data, socket->sock_data);
//                         }
//                     }
//                 }
//             }
//         }

//         // Call the periodic on_tick callback
//         if (event_mgr->cb_config.on_tick_cb) {
//             event_mgr->cb_config.on_tick_cb((struct net_event_manager_t *)event_mgr, event_mgr->app_data);
//         }
//     }

//     return NULL;
// }
