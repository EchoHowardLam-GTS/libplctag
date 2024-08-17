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

#include "compat.h"
#include "status.h"

/*
 * Define thin wrappers around native POSIX and Windows mutex functions.
 */


#if defined(IS_WINDOWS)
    #include <processthreadsapi.h>
    typedef HANDLE mutex_t;
#else
    #include <pthread.h>
    typedef pthread_mutex_t mutex_t;
#endif


static inline status_t mutex_create(mutex_t *mut)
{
    status_t rc = STATUS_OK;

#if defined(IS_WINDOWS)
    *mut = CreateMutex(NULL,                  /* default security attributes  */
                     FALSE,                  /* initially not owned          */
                     NULL);                  /* unnamed mutex                */
    if(!*mut) { rc = STATUS_ERR_OP_FAILED; }
#else
    if(pthread_mutex_init(mut, NULL)) { rc = STATUS_ERR_OP_FAILED; }
#endif

    return rc;
}


static inline status_t mutex_lock(mutex_t *mut)
{
    status_t rc = STATUS_OK;

#if defined(IS_WINDOWS)
    DWORD dwWaitResult = ~WAIT_OBJECT_0;;

    while(dwWaitResult != WAIT_OBJECT_0) {
        dwWaitResult = WaitForSingleObject(mut, INFINITE);
    }
#else
    if(pthread_mutex_lock(mut)) {
        rc = STATUS_ERR_OP_FAILED;
    }
#endif

    return rc;
}


static inline status_t mutex_unlock(mutex_t *mut)
{
    status_t rc = STATUS_OK;

#if defined(IS_WINDOWS)
    if(!ReleaseMutex(mut)) {
        rc = STATUS_ERR_OP_FAILED;
    }
#else
    if(pthread_mutex_unlock(mut)) {
        rc = STATUS_ERR_OP_FAILED;
    }
#endif

    return rc;
}


static inline status_t mutex_destroy(mutex_t *mut)
{
    status_t rc = STATUS_OK;

#if defined(IS_WINDOWS)
    CloseHandle(mut);
#else
    if(pthread_mutex_destroy(mut)) {
        rc = STATUS_ERR_OP_FAILED;
    }
#endif

    return rc;
}
