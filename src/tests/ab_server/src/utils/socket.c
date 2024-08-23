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

#include "compat.h"

#if IS_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <errno.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "slice.h"
#include "socket.h"
#include "time_utils.h"


/* lengths for socket read and write. */
#ifdef IS_MSVC
#else
    typedef size_t sock_io_len_t;
    typedef struct timeval TIMEVAL;
#endif



#define LISTEN_QUEUE (10)

#ifdef IS_WINDOWS
    typedef int sock_io_len_t;

    #define SOCK_BUF_LEN_TYPE int

    #define SOCKET_LIB_INIT                                         \
        do {                                                        \
            /* Windows needs special initialization. */             \
            static WSADATA winsock_data;                            \
            sock_rc = WSAStartup(MAKEWORD(2, 2), &winsock_data);    \
            if(sock_rc != NO_ERROR) {                               \
                info("WSAStartup failed with error: %d!", sock_rc); \
                return STATUS_INTERNAL_FAILURE;                     \
            }                                                       \
        } while(0)

    #define SOCKET_LIB_SHUTDOWN do { if(WSACleanup() != NO_ERROR) { return PLCTAG_ERR_WINSOCK; } } while(0)

    #define IS_ERR_RESULT(s) ((s) == INVALID_SOCKET)

    #define LAST_SOCK_ERR (WSAGetLastError())

#else
    typedef size_t sock_io_len_t;
    typedef struct timeval TIMEVAL;

    #define SOCK_BUF_LEN_TYPE size_t

    #define SOCKET_LIB_INIT do { } while(0)

    #define SOCKET_LIB_SHUTDOWN do { } while(0)

    #define IS_ERR_RESULT(s) ((s) < 0)

    #define LAST_SOCK_ERR (errno)

    #include <sys/select.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <stdio.h>

#endif


typedef enum {
    SOCKET_WAIT_FOR_READ_READY,
    SOCKET_WAIT_FOR_WRITE_READY,
} socket_wait_type_t;

static status_t get_sock_error(void);


status_t socket_event_wait(SOCKET sock, socket_event_t events_wanted, socket_event_t *events_found, uint32_t timeout_ms)
{
    status_t rc = STATUS_OK;

    do {
        fd_set read_fds;
        fd_set *read_fds_ptr = NULL;
        fd_set write_fds;
        fd_set *write_fds_ptr = NULL;
        struct timeval timeout;
        int result = 0;
        int option = 0;
        bool read_event = (events_wanted & SOCKET_EVENT_READ) ? true : false;
        bool write_event = (events_wanted & SOCKET_EVENT_WRITE) ? true : false;
        bool accept_event = (events_wanted & SOCKET_EVENT_ACCEPT) ? true : false;

        /* what kind of socket is this? */
        result = getsockopt(sock, SOL_SOCKET, SO_ACCEPTCONN, &option, sizeof(option));
        if(result == 0) {
            if(option != 0) {
                /* this is a listening socket */
                if(read_event || write_event) {
                    warn("This function was called asking for read/write events on a listening socket!");
                    rc = STATUS_BAD_INPUT;
                    break;
                }
            } else {
                /* this is a normal socket */
                if(accept_event) {
                    warn("This function was called asking for accept events on a non-listening socket!");
                    rc = STATUS_BAD_INPUT;
                    break;
                }
            }
        } else {
            /* getsockopt() returned an error! */
            rc = get_sock_error();
            break;
        }

        *events_found = SOCKET_EVENT_NONE;

        /* always add the socket to the read events so that we can catch socket closures. */
        if(read_event || accept_event) {
            FD_ZERO(&read_fds);
            FD_SET(sock, &read_fds);
            read_fds_ptr = &read_fds;
        }

        if(write_event) {
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            write_fds_ptr = &write_fds;
        }

        timeout.tv_sec = (timeout_ms / 1000);
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        result = select(sock + 1, read_fds_ptr, write_fds_ptr, NULL, &timeout);

        if(result < 0) {
            warn( "select() returned status %d!", result);
            rc = get_sock_error();
            break;
        }

        if(result == 0) {
            info("Timed out waiting for an event.");
            *events_found = SOCKET_EVENT_TIMEOUT;
            break;
        }

        /* so result was > 0 */
        if(read_fds_ptr && FD_ISSET(sock, read_fds_ptr)) {
            if(accept_event) {
                *events_found |= SOCKET_EVENT_ACCEPT;
            } else if(read_event) {
                *events_found |= SOCKET_EVENT_READ;
            } else {
                warn("Socket was ready for reading/accepting but we did not ask for that event!");
                rc = STATUS_INTERNAL_FAILURE;
            }
        }

        if(write_fds_ptr && FD_ISSET(sock, write_fds_ptr)) {
            if(write_event) {
                *events_found |= SOCKET_EVENT_WRITE;
            } else {
                warn("Socket was ready for writing but we did not ask for that event!");
                rc = STATUS_INTERNAL_FAILURE;
            }
        }
    } while(0);

    return rc;
}



status_t socket_open(const char *host, const char *port, bool is_server, SOCKET *sock_arg)
{
    status_t rc = STATUS_OK;
	struct addrinfo addr_hints;
    struct addrinfo *addr_info = NULL;
	int sock;
    int sock_opt = 0;
    int sock_rc;



    do {
        SOCKET_LIB_INIT;

        /*
        * Set up the hints for the type of socket that we want.
        */

        /* make sure the whole struct is set to 0 bytes */
        memset(&addr_hints, 0, sizeof addr_hints);

        /*
        * From the man page (node == host name here):
        *
        * "If the AI_PASSIVE flag is specified in hints.ai_flags, and node is NULL, then
        * the returned socket addresses will be suitable for bind(2)ing a socket that
        * will  accept(2) connections.   The returned  socket  address  will  contain
        * the  "wildcard  address" (INADDR_ANY for IPv4 addresses, IN6ADDR_ANY_INIT for
        * IPv6 address).  The wildcard address is used by applications (typically servers)
        * that intend to accept connections on any of the host's network addresses.  If
        * node is not NULL, then the AI_PASSIVE flag is ignored.
        *
        * If the AI_PASSIVE flag is not set in hints.ai_flags, then the returned socket
        * addresses will be suitable for use with connect(2), sendto(2), or sendmsg(2).
        * If node is NULL, then the  network address  will  be  set  to the loopback
        * interface address (INADDR_LOOPBACK for IPv4 addresses, IN6ADDR_LOOPBACK_INIT for
        * IPv6 address); this is used by applications that intend to communicate with
        * peers running on the same host."
        *
        * So we can get away with just setting AI_PASSIVE.
        */
        addr_hints.ai_flags = AI_PASSIVE;

        /* this allows for both IPv4 and IPv6 */
        addr_hints.ai_family = AF_UNSPEC;

        /* we want a TCP socket.  And we want it now! */
        addr_hints.ai_socktype = SOCK_STREAM;

        /* Get the address info about the local system, to be used later. */
        sock_rc = getaddrinfo(host, port, &addr_hints, &addr_info);
        if (sock_rc != 0) {
            warn("getaddrinfo() failed: %s!", gai_strerror(sock_rc));
            rc = STATUS_INTERNAL_FAILURE;
            break;
        }

        /* finally, finally, finally, we get to open a socket! */
        sock = (int)socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);

        if (sock < 0) {
            warn("socket() failed: %s!", gai_strerror(sock));
            rc = STATUS_INTERNAL_FAILURE;
            break;
        }

        /* if this is going to be a server socket, bind it. */
        if(is_server || strcmp(host,"0.0.0.0") == 0) {
            info("socket_open() setting up server socket.   Binding to address 0.0.0.0.");

            sock_rc = bind(sock, addr_info->ai_addr, (socklen_t)(unsigned int)addr_info->ai_addrlen);
            if (sock_rc < 0)	{
                warn("Unable to bind() socket: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }

            sock_rc = listen(sock, LISTEN_QUEUE);
            if(sock_rc < 0) {
                warn("Unable to call listen() on socket: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }

            /* set up our socket to allow reuse if we crash suddenly. */
            sock_opt = 1;
            sock_rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&sock_opt, sizeof(sock_opt));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_REUSEADDR on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }
        } else {
            struct timeval timeout; /* used for timing out connections etc. */
            struct linger so_linger; /* used to set up short/no lingering after connections are close()ed. */

#ifdef SO_NOSIGPIPE
            /* On *BSD and macOS, set the socket option to prevent SIGPIPE. */
            sock_rc = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (char*)&sock_opt, sizeof(sock_opt));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_REUSEADDR on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }
#endif

            timeout.tv_sec = 10;
            timeout.tv_usec = 0;

            sock_rc = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_RCVTIMEO on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }

            sock_rc = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_SNDTIMEO on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }

            /* abort the connection on close. */
            so_linger.l_onoff = 1;
            so_linger.l_linger = 0;

            sock_rc = setsockopt(sock, SOL_SOCKET, SO_LINGER,(char*)&so_linger,sizeof(so_linger));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_LINGER on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }
        }
    } while(0);

    /* free the memory for the address info struct. */
    freeaddrinfo(addr_info);

    return rc;
}



void socket_close(int sock)
{
    if(sock >= 0) {
#ifdef IS_WINDOWS
        closesocket(sock);
#else
        close(sock);
#endif
    }
}


status_t socket_accept(SOCKET listen_sock, SOCKET *client_fd, uint32_t timeout_ms)
{
    status_t rc = STATUS_OK;
    int accept_rc = 0;

    do {
        /*
        * Try to accept immediately.   If we get data, we skip any other
        * delays.   If we do not, then see if we have a timeout.
        */

        /* The listen_socket is non-blocking. */
        accept_rc = (int)accept(listen_sock, NULL, NULL);
        if(accept_rc > 0) {
            detail("Accepted new client connection.");
            *client_fd = accept_rc;
            rc = STATUS_OK;
            break;
        } else if(accept_rc < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                if(timeout_ms > 0) {
                    detail("Immediate accept attempt did not succeed, now wait for select().");
                } else {
                    detail("Accept resulted in no data.");
                }

                accept_rc = 0;
            } else {
                warn("Socket accept error: rc=%d, errno=%d", rc, errno);
                rc = STATUS_INTERNAL_FAILURE;
                break;
            }
        }

        /* only wait if we have a timeout and no error. */
        if(accept_rc == 0 && timeout_ms > 0) {
            rc = wait_for_socket_ready(listen_sock, SOCKET_WAIT_FOR_READ_READY, timeout_ms);

            if(rc == STATUS_OK) {
                accept_rc = accept(listen_sock, NULL, NULL);

                if(accept_rc > 0) {
                    *client_fd = accept_rc;
                    rc = STATUS_OK;
                    break;
                } else {
                    warn("Error accepting new connection!");
                    rc = STATUS_INTERNAL_FAILURE;
                }
            }
        }

        if(accept_rc > 0) {
            *client_fd = accept_rc;
            rc = STATUS_OK;
        }
    } while(0);

    detail("Done: result %s.", status_to_str(rc));

    return rc;
}



status_t socket_read(int sock, slice_p read_buffer, uint32_t timeout_ms)
{
    status_t rc = STATUS_OK;
    int read_amt = 0;

    info("Starting.");

    do {
        rc = wait_for_socket_read_ready(sock, timeout_ms);

        if(rc == STATUS_OK) {
            rc = STATUS_PARTIAL;

            read_amt = (int)read(sock,read_buffer->start,(size_t)slice_get_len(read_buffer));

            if(read_amt > 0) {
                /* got data. */
                slice_truncate_to_offset(read_buffer, read_amt);
                rc = STATUS_OK;
                break;
            } else if(read_amt < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                warn("Socket read error: read_amt=%d, errno=%d", read_amt, errno);
                rc = STATUS_INTERNAL_FAILURE;
            } else {
                info("No data read.");
                rc = STATUS_PARTIAL;
            }
        }
    } while(0);

    detail("Done: result %d.", rc);

    return rc;
}



/* this blocks until all the data is written or there is an error. */
status_t socket_write(int sock, slice_p data)
{
    status_t rc = STATUS_PARTIAL;
    int write_amt = 0;

    info("socket_write(): writing data:");
    debug_dump_buf(DEBUG_INFO, data->start, data->end);

    do {
        slice_t tmp_data = {0};

        write_amt = (int)write(sock, data->start, (size_t)slice_get_len(data));
        if(write_amt < 0) {
            if(errno != EAGAIN || errno != EWOULDBLOCK) {
                warn("Socket write error: rc=%d, errno=%d", rc, errno);
                rc = STATUS_INTERNAL_FAILURE;
                break;
            } else {
                /* we did not write any data or we were told we would block. */
                rc = STATUS_WOULD_BLOCK;
                break;
            }
        }

        if(write_amt == 0) {
            info("No data written, but no errors.");
            rc = STATUS_PARTIAL;
            break;
        }

        /* we wrote something. */
        info("Wrote %d bytes out of %"PRIu32" of data.", write_amt, slice_get_len(data));

        /* trim off the amount we wrote. */
        rc = slice_split_at_offset(data, (uint32_t)write_amt, NULL, &tmp_data);
        if(rc != STATUS_OK) {
            warn("Error %s trying to split data slice!", status_to_str(rc));
            break;
        }

        *data = tmp_data;

        if(slice_get_len(&tmp_data) > 0) {
            info("Did not write all the data.");
            rc = STATUS_PARTIAL;
        }
    } while(0);

    detail("Done: wrote %d bytes with result status %s.", write_amt, status_to_str(rc));

    return rc;
}



status_t get_sock_error(void)
{
    status_t rc = STATUS_OK;

    switch(LAST_SOCK_ERR) {
        case EBADF: /* bad file descriptor */
            warn( "Bad file descriptor!");
            rc = STATUS_NO_RESOURCE;
            break;

        case EINTR: /* signal was caught, this should not happen! */
            warn( "A signal was caught in a socket operation and this should not happen!");
            rc = STATUS_INTERNAL_FAILURE;
            break;

        case EINVAL: /* number of FDs was negative or exceeded the max allowed. */
            warn( "The number of fds passed to select() was negative or exceeded the allowed limit or the timeout is invalid!");
            rc = STATUS_BAD_INPUT;
            break;

        case ENOMEM: /* No mem for internal tables. */
            warn( "Insufficient memory to perform the function!");
            rc = STATUS_NO_RESOURCE;
            break;

        default:
            warn( "Unexpected socket err %d!", errno);
            rc = STATUS_INTERNAL_FAILURE;
            break;
    }

    return rc;
}