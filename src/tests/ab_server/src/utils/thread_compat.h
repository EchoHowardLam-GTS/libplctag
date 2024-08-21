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

#include <stdbool.h>

#include "status.h"


#if defined(IS_WINDOWS)
    #include <processthreadsapi.h>

    #define THREAD_FUNC(name, arg) DWORD __stdcall name(LPVOID arg)
    #define THREAD_RETURN(val) return (DWORD)(val)
    typedef HANDLE thread_t;
    typedef DWORD __stdcall (*thread_func_t)(LPVOID arg);
    typedef LPVOID thread_arg_t;
#else
    #include <pthread.h>
    #define THREAD_FUNC(name, arg) void *name(void *arg)
    #define THREAD_RETURN(val) return (val)
    typedef pthread_t thread_t;
    typedef void *(*thread_func_t)(void *arg);
    typedef void *thread_arg_t;
#endif


static inline status_t thread_create(thread_t *t, thread_func_t func, thread_arg_t arg)
{
    status_t rc = STATUS_OK;

    if(!t) {
        return STATUS_NULL_PTR;
    }

    *t = NULL;

#if defined(IS_WINDOWS)
    *t = CreateThread(NULL,           /* default security attributes */
                    0,               /* use default stack size      */
                    func,            /* thread function             */
                    arg,             /* argument to thread function */
                    (DWORD)0,        /* use default creation flags  */
                    (LPDWORD)NULL);  /* do not need thread ID       */
    /* detatch so that the thread is cleaned up on exit. */
    if(*t) { CloseHandle(t); rc = STATUS_INTERNAL_FAILURE; }
    else { rc = true; }
#else
    if(!pthread_create(t, NULL, func, arg)) {
        pthread_detach(*t);
    } else {
        rc = STATUS_INTERNAL_FAILURE;
    }
#endif

    return rc;
}
