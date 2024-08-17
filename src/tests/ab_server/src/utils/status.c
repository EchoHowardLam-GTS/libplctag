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

        case STATUS_ERR_NULL_PTR:  return "STATUS_ERR_NULL_PTR.  One or more internal arguments were NULL."; break;
        case STATUS_ERR_RESOURCE:  return "STATUS_ERR_RESOURCE. Insufficient or bad resource."; break;
        case STATUS_ERR_NOT_FOUND:  return "STATUS_ERR_NOT_FOUND. The requested item was not found."; break;
        case STATUS_ERR_NOT_RECOGNIZED:  return "STATUS_ERR_NOT_RECOGNIZED. The requested operation was not recognized."; break;
        case STATUS_ERR_NOT_SUPPORTED:  return "STATUS_ERR_NOT_SUPPORTED.  The requested operation was recognized but not supported."; break;
        case STATUS_ERR_PARAM:  return "STATUS_ERR_PARAM.  The value of a parameter is not supported or usable."; break;
        case STATUS_ERR_OP_FAILED: return "STATUS_ERR_OP_FAILED.  An operation failed."; break;
        case STATUS_ERR_TIMEOUT: return "STATUS_ERR_TIMEOUT. A timeout was reached waiting for an operation to complete."; break;
        case STATUS_ERR_ABORTED: return "STATUS_ERR_ABORTED.  The operation was aborted externally."; break;
        case STATUS_ERR_BUSY: return "STATUS_ERR_BUSY. An operation is already underway."; break;

        default: return "Unknown status code!"; break;
    }
}
