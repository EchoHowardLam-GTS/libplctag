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

#include "utils/compat.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arg_parser.h"
#include "plc.h"
#include "utils/debug.h"
#include "utils/status.h"


static status_t process_plc_arg(int argc, const char **argv, plc_connection_p template_connection);
static status_t process_path_arg(int argc, const char **argv, plc_connection_p template_connection);
static status_t process_port_arg(int argc, const char **argv, plc_connection_p template_connection);
static status_t process_debug_arg(int argc, const char **argv, plc_connection_p template_connection);
static status_t process_reject_fo_arg(int argc, const char **argv, plc_connection_p template_connection);
static status_t process_delay_arg(int argc, const char **argv, plc_connection_p template_connection);
static status_t process_tag_args(int argc, const char **argv, plc_connection_p template_connection);
static status_t parse_pccc_tag(const char *tag, plc_connection_p template_connection);
static status_t parse_cip_tag(const char *tag, plc_connection_p template_connection);




status_t process_args(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        /* process the debug level before anything else so that warnings can be seen. */
        rc = process_debug_arg(argc, argv, template_connection);
        if(rc != STATUS_OK) {
            break;
        }

        /* get the PLC type before other PLC-related arguments. */
        rc = process_plc_arg(argc, argv, template_connection);
        if(rc != STATUS_OK) {
            break;
        }

        /*
         * Process the path arg. This must be processed after we
         * know what kind of PLC we are emulating since some PLCs
         * need paths and some do not.
         */
        rc = process_path_arg(argc, argv, template_connection);
        if(rc != STATUS_OK) {
            break;
        }

        rc = process_reject_fo_arg(argc, argv, template_connection);
        if(rc != STATUS_OK) {
            break;
        }

        rc = process_delay_arg(argc, argv, template_connection);
        if(rc != STATUS_OK) {
            break;
        }

        /* now process the tags */
        rc = process_tag_args(argc, argv, template_connection);
        if(rc != STATUS_OK) {
            break;
        }
    } while(0);

    return rc;
}


status_t process_plc_arg(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;
    const char *plc_arg = NULL;

    /* scan through all the args to find all the PLC args.  Should only be one! */
    do {
        int plc_arg_count = 0;
        int plc_last_index = -1;

        for(int arg_index = 0; arg_index < argc; arg_index++) {
            if(strncmp(argv[arg_index],"--plc=",6) == 0) {
                plc_arg_count++;
                plc_last_index = arg_index;
            }
        }

        /* now check. */
        if(plc_arg_count != 1) {
            warn("You must have one, and only one, PLC command line argument!");
            rc = STATUS_ERR_PARAM;
            break;
        }

        plc_arg = argv[plc_last_index];

        /* set the PLC type to empty initially */
        template_connection->plc_type = PLC_TYPE_NONE;

        if(str_cmp_i(plc_arg, "ControlLogix") == 0) {
            warn("Selecting ControlLogix simulator.");
            template_connection->plc_type = PLC_TYPE_CONTROL_LOGIX;
            template_connection->path[0] = (uint8_t)0x00; /* filled in later. */
            template_connection->path[1] = (uint8_t)0x00; /* filled in later. */
            template_connection->path[2] = (uint8_t)0x20;
            template_connection->path[3] = (uint8_t)0x02;
            template_connection->path[4] = (uint8_t)0x24;
            template_connection->path[5] = (uint8_t)0x01;
            template_connection->path_len = 6;
            template_connection->client_to_server_max_packet = 502;
            template_connection->server_to_client_max_packet = 502;
            template_connection->needs_path = true;
        } else if(str_cmp_i(plc_arg, "Micro800") == 0) {
            warn("Selecting Micro8xx simulator.");
            template_connection->plc_type = PLC_TYPE_MICRO800;
            template_connection->path[0] = (uint8_t)0x20;
            template_connection->path[1] = (uint8_t)0x02;
            template_connection->path[2] = (uint8_t)0x24;
            template_connection->path[3] = (uint8_t)0x01;
            template_connection->path_len = 4;
            template_connection->client_to_server_max_packet = 504;
            template_connection->server_to_client_max_packet = 504;
            template_connection->needs_path = false;
        } else if(str_cmp_i(plc_arg, "Omron") == 0) {
            warn("Selecting Omron NJ/NX simulator.");
            template_connection->plc_type = PLC_TYPE_OMRON;
            template_connection->path[0] = (uint8_t)0x12;  /* Extended segment, port A */
            template_connection->path[1] = (uint8_t)0x09;  /* 9 bytes length. */
            template_connection->path[2] = (uint8_t)0x31;  /* '1' */
            template_connection->path[3] = (uint8_t)0x32;  /* '2' */
            template_connection->path[4] = (uint8_t)0x37;  /* '7' */
            template_connection->path[5] = (uint8_t)0x2e;  /* '.' */
            template_connection->path[6] = (uint8_t)0x30;  /* '0' */
            template_connection->path[7] = (uint8_t)0x2e;  /* '.' */
            template_connection->path[8] = (uint8_t)0x30;  /* '0' */
            template_connection->path[9] = (uint8_t)0x2e;  /* '.' */
            template_connection->path[10] = (uint8_t)0x31; /* '1' */
            template_connection->path[11] = (uint8_t)0x00; /* padding */
            template_connection->path[12] = (uint8_t)0x20;
            template_connection->path[13] = (uint8_t)0x02;
            template_connection->path[14] = (uint8_t)0x24;
            template_connection->path[15] = (uint8_t)0x01;
            template_connection->path_len = 16;
            template_connection->client_to_server_max_packet = 504;
            template_connection->server_to_client_max_packet = 504;
            template_connection->needs_path = false;
        } else if(str_cmp_i(plc_arg, "PLC/5") == 0) {
            warn("Selecting PLC/5 simulator.");
            template_connection->plc_type = PLC_TYPE_PLC5;
            template_connection->path[0] = (uint8_t)0x20;
            template_connection->path[1] = (uint8_t)0x02;
            template_connection->path[2] = (uint8_t)0x24;
            template_connection->path[3] = (uint8_t)0x01;
            template_connection->path_len = 4;
            template_connection->client_to_server_max_packet = 244;
            template_connection->server_to_client_max_packet = 244;
            template_connection->needs_path = false;
        } else if(str_cmp_i(plc_arg, "SLC500") == 0) {
            warn("Selecting SLC 500 simulator.");
            template_connection->plc_type = PLC_TYPE_SLC;
            template_connection->path[0] = (uint8_t)0x20;
            template_connection->path[1] = (uint8_t)0x02;
            template_connection->path[2] = (uint8_t)0x24;
            template_connection->path[3] = (uint8_t)0x01;
            template_connection->path_len = 4;
            template_connection->client_to_server_max_packet = 244;
            template_connection->server_to_client_max_packet = 244;
            template_connection->needs_path = false;
        } else if(str_cmp_i(plc_arg, "Micrologix") == 0) {
            warn("Selecting Micrologix simulator.");
            template_connection->plc_type = PLC_TYPE_MICROLOGIX;
            template_connection->path[0] = (uint8_t)0x20;
            template_connection->path[1] = (uint8_t)0x02;
            template_connection->path[2] = (uint8_t)0x24;
            template_connection->path[3] = (uint8_t)0x01;
            template_connection->path_len = 4;
            template_connection->client_to_server_max_packet = 244;
            template_connection->server_to_client_max_packet = 244;
            template_connection->needs_path = false;
        } else {
            warn("Unsupported PLC type %s!", plc_arg);
            rc = STATUS_ERR_NOT_SUPPORTED;
        }
    } while(0);

    return rc;
}



status_t process_path_arg(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        int path_arg_count = 0;
        const char *path_str = NULL;
        int tmp_path[2];

        for(int arg_index = 0; arg_index < argc; arg_index++) {
            if(strncmp(argv[arg_index],"--path=",7) == 0) {
                path_arg_count++;
                path_str = &argv[arg_index][7];
            }
        }

        /*
         * Now check how many we got.   If we have a PLC that needs a path,
         * we must have one.  If the PLC type does not want a path, we should
         * have zero.
         */

        if(template_connection->needs_path) {
            if(path_arg_count != 1) {
                warn("You must have one and only one path for this kind of PLC!");
                rc = STATUS_ERR_PARAM;
                break;
            }
        } else {
            if(path_arg_count > 0) {
                warn("This kind of PLC does not need a path.  You should have no path arguments.");
                rc = STATUS_ERR_PARAM;
                break;
            }
        }

        if (str_scanf(path_str, "%d,%d", &tmp_path[0], &tmp_path[1]) != 2) {
            warn("Error processing path \"%s\"!  Path must be two numbers separated by a comma.", path_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        template_connection->path[0] = (uint8_t)tmp_path[0];
        template_connection->path[1] = (uint8_t)tmp_path[1];

        info("Processed path %d,%d.", template_connection->path[0], template_connection->path[1]);
    } while(0);

    return rc;
}



status_t process_port_arg(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        int port_arg_count = 0;
        const char *port_str = NULL;

        for(int arg_index = 0; arg_index < argc; arg_index++) {
            if(strncmp(argv[arg_index],"--port=",7) == 0) {
                port_arg_count++;
                port_str = &argv[arg_index][7];
            }
        }

        if(port_arg_count > 1) {
            warn("You may have one or no port arguments.  The port is optional.");
            rc = STATUS_ERR_PARAM;
            break;
        }

        template_connection->port_string;
    } while(0);

    return rc;
}



status_t process_debug_arg(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        int debug_arg_count = 0;
        const char *debug_str = NULL;
        int debug_level = DEBUG_NONE;

        for(int arg_index = 0; arg_index < argc; arg_index++) {
            if(strncmp(argv[arg_index],"--debug=",8) == 0) {
                debug_arg_count++;
                debug_str = &argv[arg_index][8];
            }
        }

        if(debug_arg_count > 1) {
            warn("Setting the debug level is optional. You may have one or no debug arguments.");
            rc = STATUS_ERR_PARAM;
            break;
        }

        debug_level = atoi(debug_str);

        if(debug_level >= DEBUG_NONE || debug_level <= DEBUG_FLOOD) {
            warn("Invalid debug level!  The debug level must be between 0 and 4, inclusive.");
            rc = STATUS_ERR_PARAM;
        }

        debug_set_level(debug_level);
    } while(0);

    return rc;
}



status_t process_reject_fo_arg(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        int reject_fo_arg_count = 0;
        const char *reject_fo_str = NULL;

        for(int arg_index = 0; arg_index < argc; arg_index++) {
            if(strncmp(argv[arg_index],"--reject_fo=", 12) == 0) {
                reject_fo_arg_count++;
                reject_fo_str = &argv[arg_index][12];
            }
        }

        if(reject_fo_arg_count > 1) {
            warn("Setting the reject Foward Open value is optional. You may have one or no reject arguments.");
            rc = STATUS_ERR_PARAM;
            break;
        }

        info("Setting reject ForwardOpen count to %d.", atoi(reject_fo_str));
        template_connection->reject_fo_count = atoi(reject_fo_str);
    } while(0);

    return rc;
}



status_t process_delay_arg(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        int delay_arg_count = 0;
        const char *delay_str = NULL;

        for(int arg_index = 0; arg_index < argc; arg_index++) {
            if(strncmp(argv[arg_index],"--delay=", 8) == 0) {
                delay_arg_count++;
                delay_str = &argv[arg_index][8];
            }
        }

        if(delay_arg_count > 1) {
            warn("Setting the delay value is optional. You may have one or no delay arguments.");
            rc = STATUS_ERR_PARAM;
            break;
        }

        info("Setting response delay to %dms.", atoi(delay_str));
        template_connection->response_delay = atoi(delay_str);
    } while(0);

    return rc;
}




status_t process_tag_args(int argc, const char **argv, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;

    do {
        int tag_arg_count = 0;
        const char *tag_str = NULL;

        for(int arg_index = 0; arg_index < argc && rc == STATUS_OK; arg_index++) {
            if(strncmp(argv[arg_index],"--tag=", 6) == 0) {
                tag_arg_count++;
                tag_str = &argv[arg_index][6];

                if((template_connection->plc_type == PLC_TYPE_PLC5 || template_connection->plc_type == PLC_TYPE_SLC || template_connection->plc_type == PLC_TYPE_MICROLOGIX)) {
                    rc = parse_pccc_tag(tag_str, template_connection);
                } else {
                    rc = parse_cip_tag(tag_str, template_connection);
                }
            }
        }

        if(tag_arg_count < 1) {
            warn("You must have at least one tag argument.");
            rc = STATUS_ERR_PARAM;
            break;
        }
    } while(0);

    return rc;
}




/*
 * PCCC tags are in the format:
 *    <data file>[<size>]
 *
 * Where data file is one of the following:
 *     N7 - 2 byte signed integer.  Requires size.
 *     F8 - 4-byte floating point number.   Requires size.
 *     ST18 - 82-byte string with 2-byte count word.
 *     L19 - 4 byte signed integer.   Requires size.
 *
 * The size field is a single positive integer.
 */

status_t parse_pccc_tag(const char *tag_str, plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;
    tag_def_p tag = calloc(1, sizeof(*tag));
    char data_file_name[200] = { 0 };
    char size_str[200] = { 0 };
    int num_dims = 0;
    size_t start = 0;
    size_t len = 0;

    info("Starting.");

    do {

        assert_error((tag), "Unable to allocate memory for new tag!");

        /* FIXME - check return code */
        mutex_create(&(tag->mutex));

        /* try to match the two parts of a tag definition string. */
        info("Match data file.");

        /* first match the data file. */
        start = 0;
        len = strspn(tag_str + start, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        if (!len) {
            warn("Unable to parse tag definition string, cannot find tag name in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* copy the string. */
        for (size_t i = 0; i < len && i < (size_t)200; i++) {
            data_file_name[i] = tag_str[start + i];
        }

        /* check data file for a match. */
        if(str_cmp_i(data_file_name, "N7") == 0) {
            info("Found N7 data file.");
            tag->tag_type = TAG_PCCC_TYPE_INT;
            tag->elem_size = 2;
            tag->data_file_num = 7;
        } else if(str_cmp_i(data_file_name, "F8") == 0) {
            info("Found F8 data file.");
            tag->tag_type = TAG_PCCC_TYPE_REAL;
            tag->elem_size = 4;
            tag->data_file_num = 8;
        } else if(str_cmp_i(data_file_name, "ST18") == 0) {
            info("Found ST18 data file.");
            tag->tag_type = TAG_PCCC_TYPE_STRING;
            tag->elem_size = 84;
            tag->data_file_num = 18;
        } else if(str_cmp_i(data_file_name, "L19") == 0) {
            info("Found L19 data file.");
            tag->tag_type = TAG_PCCC_TYPE_DINT;
            tag->elem_size = 4;
            tag->data_file_num = 19;
        } else {
            warn("Unknown data file %s, unable to create tag!", data_file_name);
            rc = STATUS_ERR_NOT_RECOGNIZED;
            break;
        }

        start += len;

        /* get the array size delimiter. */
        if(tag_str[start] != '[') {
            warn("Unable to parse tag definition string, cannot find starting square bracket after data file in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
        }

        start++;

        /* get the size field */
        len = strspn(tag_str + start, "0123456789");
        if (!len) {
            warn("Unable to parse tag definition string, cannot match array size in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* copy the string. */
        for (size_t i = 0; i < len && i < (size_t)200; i++) {
            size_str[i] = tag_str[start + i];
        }

        start += len;

        if (tag_str[start] != ']') {
            warn("Unable to parse tag definition string, cannot find ending square bracket after size in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* make sure all the dimensions are defaulted to something sane. */
        tag->dimensions[0] = 1;
        tag->dimensions[1] = 1;
        tag->dimensions[2] = 1;

        /* match the size. */
        num_dims = str_scanf(size_str, "%zu", &tag->dimensions[0]);
        if(num_dims != 1) {
            warn("Unable to parse tag size in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* check the size. */
        if(tag->dimensions[0] <= 0) {
            warn("The array size must least 1 and may not be negative!");
            rc = STATUS_ERR_PARAM;
            break;
        }

        tag->elem_count = tag->dimensions[0];
        tag->num_dimensions = 1;

        /* copy the tag name */
        tag->name = strdup(data_file_name);
        if (!tag->name) {
            warn("Unable to allocate a copy of the data file \"%s\"!", data_file_name);
            rc = STATUS_ERR_RESOURCE;
            break;
        }

        /* allocate the tag data array. */
        info("allocating %d elements of %d bytes each.", tag->elem_count, tag->elem_size);
        tag->data = calloc(tag->elem_count, (size_t)tag->elem_size);
        if(!tag->data) {
            warn("Unable to allocate tag data buffer!");
            free(tag->name);
            rc = STATUS_ERR_RESOURCE;
            break;
        }

        info("Processed \"%s\" into tag %s of type %x with dimensions (%d, %d, %d).", tag_str, tag->name, tag->tag_type, tag->dimensions[0], tag->dimensions[1], tag->dimensions[2]);

        /* add the tag to the list. */
        tag->next_tag = template_connection->tags;
        template_connection->tags = tag;
    } while(0);

    return rc;
}




/*
 * CIP tags are in the format:
 *    <name>:<type>[<sizes>]
 *
 * Where name is alphanumeric, starting with an alpha character.
 *
 * Type is one of:
 *     INT - 2-byte signed integer.  Requires array size(s).
 *     DINT - 4-byte signed integer.  Requires array size(s).
 *     LINT - 8-byte signed integer.  Requires array size(s).
 *     REAL - 4-byte floating point number.  Requires array size(s).
 *     LREAL - 8-byte floating point number.  Requires array size(s).
 *     STRING - 82-byte string with 4-byte count word and 2 bytes of padding.
 *     BOOL - single bit returned as a byte.
 *
 * Array size field is one or more (up to 3) numbers separated by commas.
 */

status_t parse_cip_tag(const char *tag_str,plc_connection_p template_connection)
{
    status_t rc = STATUS_OK;
    tag_def_p tag = calloc(1, sizeof(*tag));
    char tag_name[200] = { 0 };
    char type_str[200] = { 0 };
    char dim_str[200] = { 0 };
    int num_dims = 0;
    size_t start = 0;
    size_t len = 0;

    do {
        if(!tag) {
            warn("Unable to allocate memory for new tag!");
            rc = STATUS_ERR_RESOURCE;
            break;
        }

        /* try to match the three parts of a tag definition string. */

        /* first match the name. */
        start = 0;
        len = strspn(tag_str + start, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
        if (!len) {
            warn("Unable to parse tag definition string, cannot find tag name in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* copy the string. */
        for (size_t i = 0; i < len && i < (size_t)200; i++) {
            tag_name[i] = tag_str[start + i];
        }

        start += len;

        if(tag_str[start] != ':') {
            warn("Unable to parse tag definition string, cannot find colon after tag name in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
        }

        start++;

        /* get the type field */
        len = strspn(tag_str + start, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (!len) {
            warn("Unable to parse tag definition string, cannot match tag type in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* copy the string. */
        for (size_t i = 0; i < len && i < (size_t)200; i++) {
            type_str[i] = tag_str[start + i];
        }

        start += len;

        if (tag_str[start] != '[') {
            warn("Unable to parse tag definition string, cannot find starting square bracket after tag type in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        start++;


        /* get the dimension field */
        len = strspn(tag_str + start, "0123456789,");
        if (!len) {
            warn("Unable to parse tag definition string, cannot match dimension in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
        }

        /* copy the string. */
        for (size_t i = 0; i < len && i < (size_t)200; i++) {
            dim_str[i] = tag_str[start + i];
        }

        start += len;

        if (tag_str[start] != ']') {
            warn("Unable to parse tag definition string, cannot find ending square bracket after tag type in \"%s\"!", tag_str);
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* match the type. */
        if(str_cmp_i(type_str, "SINT") == 0) {
            tag->tag_type = TAG_CIP_TYPE_SINT;
            tag->elem_size = 1;
        } else if(str_cmp_i(type_str, "INT") == 0) {
            tag->tag_type = TAG_CIP_TYPE_INT;
            tag->elem_size = 2;
        } else if(str_cmp_i(type_str, "DINT") == 0) {
            tag->tag_type = TAG_CIP_TYPE_DINT;
            tag->elem_size = 4;
        } else if(str_cmp_i(type_str, "LINT") == 0) {
            tag->tag_type = TAG_CIP_TYPE_LINT;
            tag->elem_size = 8;
        } else if(str_cmp_i(type_str, "REAL") == 0) {
            tag->tag_type = TAG_CIP_TYPE_REAL;
            tag->elem_size = 4;
        } else if(str_cmp_i(type_str, "LREAL") == 0) {
            tag->tag_type = TAG_CIP_TYPE_LREAL;
            tag->elem_size = 8;
        } else if(str_cmp_i(type_str, "STRING") == 0) {
            tag->tag_type = TAG_CIP_TYPE_STRING;
            tag->elem_size = 88;
        } else if(str_cmp_i(type_str, "BOOL") == 0){
            tag->tag_type = TAG_CIP_TYPE_BOOL;
            tag->elem_size = 1;
        } else {
            warn("Unsupported tag type \"%s\"!", type_str);
            rc = STATUS_ERR_NOT_SUPPORTED;
            break;
        }

        /* match the dimensions. */
        tag->dimensions[0] = 0;
        tag->dimensions[1] = 0;
        tag->dimensions[2] = 0;

        num_dims = str_scanf(dim_str, "%zu,%zu,%zu,%*u", &tag->dimensions[0], &tag->dimensions[1], &tag->dimensions[2]);
        if(num_dims < 1 || num_dims > 3) {
            warn("Tag dimensions must have at least one dimension non-zero and no more than three dimensions.");
            rc = STATUS_ERR_PARAM;
            break;
        }

        /* check the dimensions. */
        if(tag->dimensions[0] <= 0) {
            warn("The first tag dimension must be at least 1 and may not be negative!");
            rc = STATUS_ERR_PARAM;
            break;
        }

        tag->elem_count = tag->dimensions[0];
        tag->num_dimensions = 1;

        if(tag->dimensions[1] > 0) {
            tag->elem_count *= tag->dimensions[1];
            tag->num_dimensions = 2;
        } else {
            tag->dimensions[1] = 1;
        }

        if(tag->dimensions[2] > 0) {
            tag->elem_count *= tag->dimensions[2];
            tag->num_dimensions = 3;
        } else {
            tag->dimensions[2] = 1;
        }

        /* copy the tag name */
        tag->name = strdup(tag_name);
        if (!tag->name) {
            warn("Unable to allocate a copy of the tag name \"%s\"!", tag_name);
            rc = STATUS_ERR_RESOURCE;
            break;
        }

        /* handle Rockwell array weirdness. */
        if(tag->tag_type == TAG_CIP_TYPE_BOOL && tag->elem_count > 1) {
            if(template_connection->plc_type == PLC_TYPE_CONTROL_LOGIX) {
                info("Changed BOOL array to conform to Rockwell's implementation.");

                /* calculate the number of 32-bit elements, round up. */
                size_t actual_element_count = (tag->elem_count + 31)/32;

                tag->elem_count = actual_element_count;
                tag->tag_type = TAG_CIP_TYPE_32BIT_BIT_STRING;
            }
        }

        // FIXME - Omron does weird things with BOOL arrays too.

        /* allocate the tag data array. */
        info("allocating %d elements of %d bytes each.", tag->elem_count, tag->elem_size);
        tag->data = calloc(tag->elem_count, (size_t)tag->elem_size);
        if(!tag->data) {
            warn("Unable to allocate tag data buffer!");
            free(tag->name);
            rc = STATUS_ERR_RESOURCE;
            break;
        }

        info("Processed \"%s\" into tag %s of type %x with dimensions (%d, %d, %d).", tag_str, tag->name, tag->tag_type, tag->dimensions[0], tag->dimensions[1], tag->dimensions[2]);

        /* add the tag to the list. */
        tag->next_tag = template_connection->tags;
        template_connection->tags = tag;
    } while(0);

    return true;
}
