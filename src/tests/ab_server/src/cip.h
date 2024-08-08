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

#include "plc.h"
#include "utils/slice.h"
#include "utils/tcp_server.h"

enum {
    CIP_OK = 0,
    CIP_ERR_FLAG = 0x01,
    CIP_ERR_PATH_DEST_UNKNOWN = 0x05,
    CIP_ERR_FRAG = 0x06,
    CIP_ERR_UNSUPPORTED = 0x08,
    CIP_ERR_INSUFFICIENT_DATA = 0x13,
    CIP_ERR_INVALID_PARAMETER = 0x20,

    CIP_ERR_EXTENDED = 0xFF,

    /* extended errors */
    CIP_ERR_EX_TOO_LONG = 0x2105,
    CIP_ERR_EX_DUPLICATE_CONN = 0x100,

};

extern int cip_dispatch_request(tcp_client_p client);
