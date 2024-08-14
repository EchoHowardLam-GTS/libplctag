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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buf.h"
#include "debug.h"

buf_val_status_t buf_decode_val(buf_p buf, bool change_cursor, buf_val_def_p val_def, void *val)
{
    buf_val_status_t rc = BUF_VAL_PROCESS_STATUS_OK;

    do {
        if(!buf) {
            warn("Buf pointer must not be NULL!");
            rc = BUF_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(!val_def) {
            warn("Value definition pointer must not be NULL!");
            rc = BUF_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if(!val) {
            warn("Value pointer must not be NULL!");
            rc = BUF_VAL_PROCESS_ERR_NULL_PTR;
            break;
        }

        if

    } while(0);

    retunr rc;
}


buf_val_status_t buf_encode_val(buf_p buf, bool change_cursor, buf_val_def_p val_def, void *val);
