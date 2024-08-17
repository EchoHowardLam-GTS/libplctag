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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "debug.h"
#include "slice.h"
#include "socket.h"
#include "time_utils.h"


/* lengths for socket read and write. */
#ifdef IS_MSVC
    typedef int sock_io_len_t;
#else
    typedef size_t sock_io_len_t;
    typedef struct timeval TIMEVAL;
#endif


#define LISTEN_QUEUE (10)

#ifdef IS_WINDOWS
    #define SOCK_BUF_LEN_TYPE int
#else
    #define SOCK_BUF_LEN_TYPE size_t
#endif




int socket_open(const char *host, const char *port, bool is_server, SOCKET *sock_arg)
{
    status_t rc = STATUS_OK;
	struct addrinfo addr_hints;
    struct addrinfo *addr_info = NULL;
	int sock;
    int sock_opt = 0;
    int sock_rc;

    do {
#ifdef IS_WINDOWS
        /* Windows needs special initialization. */
        static WSADATA winsock_data;
        sock_rc = WSAStartup(MAKEWORD(2, 2), &winsock_data);

        if (sock_rc != NO_ERROR) {
            info("WSAStartup failed with error: %d!", sock_rc);
            rc = STATUS_ERR_OP_FAILED;
            break;
        }
#endif

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
            rc = STATUS_ERR_OP_FAILED;
            break;
        }

        /* finally, finally, finally, we get to open a socket! */
        sock = (int)socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);

        if (sock < 0) {
            warn("socket() failed: %s!", gai_strerror(sock));
            rc = STATUS_ERR_OP_FAILED;
            break;
        }

        /* if this is going to be a server socket, bind it. */
        if(is_server || strcmp(host,"0.0.0.0") == 0) {
            info("socket_open() setting up server socket.   Binding to address 0.0.0.0.");

            sock_rc = bind(sock, addr_info->ai_addr, (socklen_t)(unsigned int)addr_info->ai_addrlen);
            if (sock_rc < 0)	{
                warn("Unable to bind() socket: %s!", gai_strerror(sock_rc));
                rc = STATUS_ERR_OP_FAILED;
                break;
            }

            sock_rc = listen(sock, LISTEN_QUEUE);
            if(sock_rc < 0) {
                warn("Unable to call listen() on socket: %s!", gai_strerror(sock_rc));
                rc = STATUS_ERR_OP_FAILED;
                break;
            }

            /* set up our socket to allow reuse if we crash suddenly. */
            sock_opt = 1;
            sock_rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&sock_opt, sizeof(sock_opt));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_REUSEADDR on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_ERR_OP_FAILED;
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
                rc = STATUS_ERR_OP_FAILED;
                break;
            }
#endif

            timeout.tv_sec = 10;
            timeout.tv_usec = 0;

            sock_rc = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_RCVTIMEO on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_ERR_OP_FAILED;
                break;
            }

            sock_rc = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_SNDTIMEO on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_ERR_OP_FAILED;
                break;
            }

            /* abort the connection on close. */
            so_linger.l_onoff = 1;
            so_linger.l_linger = 0;

            sock_rc = setsockopt(sock, SOL_SOCKET, SO_LINGER,(char*)&so_linger,sizeof(so_linger));
            if(sock_rc) {
                socket_close(sock);
                warn("Setting SO_LINGER on socket failed: %s!", gai_strerror(sock_rc));
                rc = STATUS_ERR_OP_FAILED;
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
        if(accept_rc < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                if(timeout_ms > 0) {
                    detail("Immediate accept attempt did not succeed, now wait for select().");
                } else {
                    detail("Read resulted in no data.");
                }

                accept_rc = 0;
            } else {
                warn("Socket accept error: rc=%d, errno=%d", rc, errno);
                rc = STATUS_ERR_OP_FAILED;
                break;
            }
        } else if(accept_rc > 0) {
            *client_fd = accept_rc;
            rc = STATUS_OK;
            break;
        }

        /* only wait if we have a timeout and no error. */
        if(accept_rc == 0 && timeout_ms > 0) {
            fd_set accept_set;
            struct timeval tv;
            int select_rc = 0;

            tv.tv_sec = (time_t)(timeout_ms / 1000);
            tv.tv_usec = (suseconds_t)(timeout_ms % 1000) * (suseconds_t)(1000);

            FD_ZERO(&accept_set);

            FD_SET(listen_sock, &accept_set);

            select_rc = select(listen_sock+1, &accept_set, NULL, NULL, &tv);
            if(select_rc > 0) {
                if(FD_ISSET(listen_sock, &accept_set)) {
                    detail("Client connected.");
                    accept_rc = accept(listen_sock, NULL, NULL);

                    if(accept_rc > 0) {
                        *client_fd = accept_rc;
                        rc = STATUS_OK;
                        break;
                    } else {
                        warn("Error accepting new connection!");
                        rc = STATUS_ERR_OP_FAILED;
                    }
                } else {
                    warn( "select() returned but the listen socket is not ready!");
                    rc = STATUS_ERR_OP_FAILED;
                }
            } else if(select_rc == 0) {
                detail("Socket accept timed out.");
                rc = STATUS_ERR_TIMEOUT;
            } else {
                warn( "select() returned status %d!", select_rc);

                switch(errno) {
                    case EBADF: /* bad file descriptor */
                        warn( "Bad file descriptor used in select()!");
                        rc = STATUS_ERR_RESOURCE;
                        break;

                    case EINTR: /* signal was caught, this should not happen! */
                        warn( "A signal was caught in select() and this should not happen!");
                        rc = STATUS_ERR_OP_FAILED;
                        break;

                    case EINVAL: /* number of FDs was negative or exceeded the max allowed. */
                        warn( "The number of fds passed to select() was negative or exceeded the allowed limit or the timeout is invalid!");
                        rc = STATUS_ERR_PARAM;
                        break;

                    case ENOMEM: /* No mem for internal tables. */
                        warn( "Insufficient memory for select() to run!");
                        rc = STATUS_ERR_RESOURCE;
                        break;

                    default:
                        warn( "Unexpected listen_socket err %d!", errno);
                        rc = STATUS_ERR_OP_FAILED;
                        break;
                }
            }

            /* try to accept again. */
            accept_rc = (int)accept(listen_sock, NULL, NULL);
            if(accept_rc < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    detail("No data accept.");
                    accept_rc = 0;
                } else {
                    warn("Socket accept error: accept_rc=%d, errno=%d", accept_rc, errno);
                    rc = STATUS_ERR_OP_FAILED;
                }
            }
        }
    } while(0);

    detail("Done: result %d.", rc);

    return rc;
}


#ifdef IS_WINDOWS

#else

status_t socket_read(int sock, slice_p buf, uint32_t timeout_ms)
{
    status_t rc = STATUS_OK;
    int read_status = 0;

    do {
        /*
        * Try to read immediately.   If we get data, we skip any other
        * delays.   If we do not, then see if we have a timeout.
        */

        /* The socket is non-blocking. */
        read_status = (int)read(sock,buf->start,(size_t)slice_get_len(buf));
        if(read_status < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                if(timeout_ms > 0) {
                    detail("Immediate read attempt did not succeed, now wait for select().");
                } else {
                    detail("Read resulted in no data.");
                }

                read_status = 0;
            } else {
                warn("Socket read error: rc=%d, errno=%d", rc, errno);
                rc = STATUS_ERR_OP_FAILED;
                break;
            }
        }

        /* only wait if we have a timeout and no error and no data. */
        if(read_status == 0 && timeout_ms > 0) {
            fd_set read_set;
            struct timeval tv;
            int select_rc = 0;

            tv.tv_sec = (time_t)(timeout_ms / 1000);
            tv.tv_usec = (suseconds_t)(timeout_ms % 1000) * (suseconds_t)(1000);

            FD_ZERO(&read_set);

            FD_SET(sock, &read_set);

            select_rc = select(sock+1, &read_set, NULL, NULL, &tv);
            if(select_rc == 1) {
                if(FD_ISSET(sock, &read_set)) {
                    detail("Socket can read data.");
                } else {
                    warn( "select() returned but socket is not ready to read data!");
                    rc = STATUS_ERR_OP_FAILED;
                }
            } else if(select_rc == 0) {
                detail("Socket read timed out.");
                rc = STATUS_ERR_TIMEOUT;
            } else {
                warn( "select() returned status %d!", select_rc);

                switch(errno) {
                    case EBADF: /* bad file descriptor */
                        warn( "Bad file descriptor used in select()!");
                        rc = STATUS_ERR_RESOURCE;
                        break;

                    case EINTR: /* signal was caught, this should not happen! */
                        warn( "A signal was caught in select() and this should not happen!");
                        rc = STATUS_ERR_OP_FAILED;
                        break;

                    case EINVAL: /* number of FDs was negative or exceeded the max allowed. */
                        warn( "The number of fds passed to select() was negative or exceeded the allowed limit or the timeout is invalid!");
                        rc = STATUS_ERR_PARAM;
                        break;

                    case ENOMEM: /* No mem for internal tables. */
                        warn( "Insufficient memory for select() to run!");
                        rc = STATUS_ERR_RESOURCE;
                        break;

                    default:
                        warn( "Unexpected socket err %d!", errno);
                        rc = STATUS_ERR_OP_FAILED;
                        break;
                }
            }

            /* try to read again. */
            read_status = (int)read(sock,buf->start,(size_t)slice_get_len(buf));
            if(read_status < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    detail("No data read.");
                    read_status = 0;
                } else {
                    warn("Socket read error: read_status=%d, errno=%d", read_status, errno);
                    rc = STATUS_ERR_OP_FAILED;
                }
            }
        }
    } while(0);

    detail("Done: result %d.", rc);

    return rc;
}

#endif


#ifdef IS_WINDOWS

#else

/* this blocks until all the data is written or there is an error. */
status_t socket_write(int sock, slice_p buf, uint32_t timeout_ms)
{
    status_t rc = STATUS_OK;

    do {

    } while(0);

    uint16_t bytes_written = 0;
    uint16_t data_len = 0;
    uint8_t *data_start = NULL;
    uint16_t bytes_to_write = 0;

    if(!buf) {
        info("WARN: Null buffer pointer!");
        rc = STATUS_ERR_OP_FAILED;
    }

    /* write until we exhaust the data. The buf length marks the end of the data. */

    info("socket_write(): writing data:");
    buf_dump(buf);

    data_start = buf_data_ptr(buf, 0);

    if(!data_start) {
        info("WARN: Null data pointer!");
        rc = STATUS_ERR_OP_FAILED;
    }

    bytes_to_write = buf_len(buf);

    do {
#ifdef IS_WINDOWS
        rc = (int)send(sock, (char *)data_start, (int)bytes_to_write, 0);
#else
        rc = (int)send(sock, (char *)data_start, (size_t)bytes_to_write, 0);
#endif

        /* was there an error? */
        if(rc < 0) {
            /*
             * check the return value.  If it is an interrupted system call
             * or would block, just keep looping.
             */
#ifdef IS_WINDOWS
            rc = WSAGetLastError();
            if(rc != WSAEWOULDBLOCK) {
#else
            rc = errno;
            if(rc != EAGAIN && rc != EWOULDBLOCK) {
#endif
                info("Socket write error rc=%d.\n", rc);
                rc = STATUS_ERR_OP_FAILED;
            } else {
                /* no error, just try again */
                rc = 0;
            }
        } else {
            bytes_written += (size_t)rc;
            data_start += rc;
            bytes_to_write = data_len - bytes_written;
        }
    } while(bytes_written < data_len);

    return (int)(unsigned int)bytes_written;
}

#endif
