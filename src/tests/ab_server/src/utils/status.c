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


#include "status.h"


const char *status_to_str(status_t status)
{
    switch(status) {
        case STATUS_OK: return "STATUS_OK.  No errors."; break;
        case STATUS_PENDING: return "STATUS_PENDING. Waiting for an operation to complete."; break;
        case STATUS_TERMINATE: return "STATUS_TERMINATE.  Shut down or shutting down."; break;
        case STATUS_WOULD_BLOCK: return "STATUS_WOULD_BLOCK. The operation would block if it was not asynchronous."; break;

        case STATUS_NOT_FOUND:  return "STATUS_NOT_FOUND. The requested item was not found."; break;
        case STATUS_NOT_FOUND + STATUS_ERROR:  return "STATUS_NOT_FOUND_ERROR: The requested item not found."; break;

        case STATUS_NOT_RECOGNIZED:  return "STATUS_NOT_RECOGNIZED. The requested operation was not recognized."; break;
        case STATUS_NOT_RECOGNIZED + STATUS_ERROR:  return "STATUS_NOT_RECOGNIZED_ERROR: The requested operation was not recognized."; break;

        case STATUS_NOT_SUPPORTED:  return "STATUS_NOT_SUPPORTED.  The requested operation was recognized but not supported."; break;
        case STATUS_NOT_SUPPORTED + STATUS_ERROR:  return "STATUS_NOT_SUPPORTED_ERROR: The requested operation was recognized but not supported."; break;

        case STATUS_BAD_INPUT:  return "STATUS_BAD_INPUT.  The value of a parameter is not supported or usable."; break;
        case STATUS_BAD_INPUT + STATUS_ERROR:  return "STATUS_BAD_INPUT_ERROR: The value of a parameter is not supported or usable."; break;

        case STATUS_ABORTED: return "STATUS_ABORTED.  The operation was aborted externally."; break;
        case STATUS_ABORTED + STATUS_ERROR: return "STATUS_ABORTED_ERROR: The operation was aborted externally."; break;

        case STATUS_BUSY: return "STATUS_BUSY. An operation is already underway."; break;
        case STATUS_BUSY + STATUS_ERROR: return "STATUS_BUSY_ERROR: An operation is already underway."; break;

        case STATUS_TIMEOUT: return "STATUS_TIMEOUT. A timeout was reached waiting for an operation to complete."; break;
        case STATUS_TIMEOUT + STATUS_ERROR: return "STATUS_TIMEOUT_ERROR: A timeout was reached waiting for an operation to complete."; break;

        case STATUS_PARTIAL: return "STATUS_PARTIAL. Incomplete data was found."; break;
        case STATUS_PARTIAL + STATUS_ERROR: return "STATUS_PARTIAL_ERROR: Incomplete data was found."; break;

        case STATUS_OUT_OF_BOUNDS: return "STATUS_OUT_OF_BOUNDS. Attempt to access data out of bounds."; break;
        case STATUS_OUT_OF_BOUNDS + STATUS_ERROR: return "STATUS_OUT_OF_BOUNDS_ERROR: Attempt to access data out of bounds."; break;

        case STATUS_NULL_PTR:  return "STATUS_NULL_PTR.  One or more internal arguments were NULL."; break;
        case STATUS_NULL_PTR + STATUS_ERROR:  return "STATUS_NULL_PTR_ERROR:  One or more internal arguments were NULL."; break;

        case STATUS_NO_RESOURCE:  return "STATUS_NO_RESOURCE. Insufficient or bad resource."; break;
        case STATUS_NO_RESOURCE + STATUS_ERROR:  return "STATUS_NO_RESOURCE. Insufficient or bad resource."; break;

        case STATUS_SETUP_FAILURE: return "STATUS_SETUP_FAILURE. Creation or configuration of a resource failed."; break;
        case STATUS_SETUP_FAILURE + STATUS_ERROR: return "STATUS_SETUP_FAILURE_ERROR: Creation or configuration of a resource failed."; break;

        case STATUS_INTERNAL_FAILURE: return "STATUS_INTERNAL_FAILURE. Something went wrong inside the code."; break;
        case STATUS_INTERNAL_FAILURE + STATUS_ERROR: return "STATUS_INTERNAL_FAILURE_ERROR: Something went wrong inside the code."; break;

        case STATUS_EXTERNAL_FAILURE: return "STATUS_EXTERNAL_FAILURE. A failure was reported outside the code."; break;
        case STATUS_EXTERNAL_FAILURE + STATUS_ERROR: return "STATUS_EXTERNAL_FAILURE_ERROR: A failure was reported outside the code."; break;

        default: return "Unknown status code!"; break;
    }
}
