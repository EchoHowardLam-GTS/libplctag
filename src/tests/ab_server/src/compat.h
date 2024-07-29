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

#pragma once

/*
 * This file contains various compatibility includes and definitions
 * to allow compilation across POSIX and Windows systems.
 */

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
    #define IS_WINDOWS (1)
#endif

#if defined(_MSC_VER)
    #define IS_MSVC (1)
#endif


#ifdef IS_MSVC
    #define str_cmp_i(first, second) _stricmp(first, second)
    #define strdup _strdup
    #define str_scanf sscanf_s
#else
    #define str_cmp_i(first, second) strcasecmp(first, second)
    #define str_scanf sscanf
#endif

/* Define ssize_t */
#ifdef IS_MSVC
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
#else
    #include <sys/types.h>
#endif




#if defined(WIN32) || defined(_WIND32)
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

    typedef pthread_mutex_t mutex_t;

#endif


static inline int thread_create(thread_t *t, thread_func_t func, thread_arg_t arg)
{
    int rc = 0;

#if defined(WIN32) || defined(_WIN32)
    *t = CreateThread(NULL,           /* default security attributes */
                    0,               /* use default stack size      */
                    func,            /* thread function             */
                    arg,             /* argument to thread function */
                    (DWORD)0,        /* use default creation flags  */
                    (LPDWORD)NULL);  /* do not need thread ID       */
    /* detatch so that the thread is cleaned up on exit. */
    if(*t) { CloseHandle(t); }
    else { rc = 1; }
#else
    if(!pthread_create(t, NULL, func, arg)) {
        pthread_detach(*t);
    } else {
        rc = 1;
    }
#endif

    return rc;
}


static inline int mutex_create(mutex_t *mut)
{
    int rc = 0;

#if defined(WIN32) || defined(_WIN32)
    *mut = CreateMutex(NULL,                  /* default security attributes  */
                     FALSE,                  /* initially not owned          */
                     NULL);                  /* unnamed mutex                */
    if(!*mut) { rc = 1; }
#else
    if(pthread_mutex_init(mut, NULL)) { rc = 1; }
#endif

    return rc;
}


static inline int mutex_lock(mutex_t *mut)
{
    int rc = 0;

#if defined(WIN32) || defined(_WIN32)
    DWORD dwWaitResult = ~WAIT_OBJECT_0;;

    while(dwWaitResult != WAIT_OBJECT_0) {
        dwWaitResult = WaitForSingleObject(mut, INFINITE);
    }
#else
    if(pthread_mutex_lock(mut)) {
        rc = 1;
    }
#endif

    return rc;
}


static inline int mutex_unlock(mutex_t *mut)
{
    int rc = 0;

#if defined(WIN32) || defined(_WIN32)
    if(!ReleaseMutex(mut)) {
        rc = 1;
    }
#else
    if(pthread_mutex_unlock(mut)) {
        rc = 1;
    }
#endif

    return rc;
}


static inline int mutex_destroy(mutex_t *mut)
{
    int rc = 0;

#if defined(WIN32) || defined(_WIN32)
    CloseHandle(mut);
#else
    if(pthread_mutex_destroy(mut)) {
        rc = 1;
    }
#endif

    return rc;
}
