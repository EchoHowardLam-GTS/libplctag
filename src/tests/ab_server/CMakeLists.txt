#   Copyright (C) 2022 by Kyle Hayes
#   Author Kyle Hayes  kyle.hayes@gmail.com
#
# This software is available under either the Mozilla Public license
# version 2.0 (MPL 2.0) or the GNU LGPL version 2 (or later) license, whichever
# you choose.
#
# MPL 2.0:
#
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#
# LGPL 2:
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU Library General Public License as
#   published by the Free Software Foundation; either version 2 of the
#   License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public
#   License along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

if("${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    set(TOOLCHAIN_FILE "toolchains/${CMAKE_SYSTEM_NAME}-${CMAKE_C_COMPILER_ID}.cmake")

    message("TOOLCHAIN_FILE = ${TOOLCHAIN_FILE}")

    include("${TOOLCHAIN_FILE}")
endif()

add_executable(ab_server "src/arg_parser.h"
                         "src/cip.c"
                         "src/cip.h"
                         "src/cpf.c"
                         "src/cpf.h"
                         "src/eip.c"
                         "src/eip.h"
                         "src/main.c"
                         "src/pccc.c"
                         "src/pccc.h"
                         "src/plc.h"
                         "src/utils/compat.h"
                         "src/utils/debug.c"
                         "src/utils/debug.h"
                         "src/utils/mutex_compat.h"
                         "src/utils/slice.c"
                         "src/utils/slice.h"
                         "src/utils/status.c"
                         "src/utils/status.h"
                         "src/utils/socket.c"
                         "src/utils/socket.h"
                         "src/utils/tcp_server.c"
                         "src/utils/tcp_server.h"
                         "src/utils/thread_compat.h"
                         "src/utils/time_utils.c"
                         "src/utils/time_utils.h"
)

message("compiler flags = \"${COMPILER_FLAGS}\"")
target_compile_options(ab_server PUBLIC ${COMPILER_FLAGS})
