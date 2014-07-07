/**
 * Copyright (c) 2013-2014 Genome Research Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "rodsClient.hpp"
#include "rodsPath.hpp"
#include <jansson.h>

#include "baton.h"
#include "config.h"
#include "json.h"
#include "log.h"

static int debug_flag;
static int acl_flag;
static int avu_flag;
static int help_flag;
static int timestamp_flag;
static int unbuffered_flag;
static int verbose_flag;
static int version_flag;

int do_list_paths(FILE *input, print_flags pflags);

int main(int argc, char *argv[]) {
    print_flags pflags = PRINT_DEFAULT;
    int exit_status = 0;
    char *json_file = NULL;
    FILE *input     = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"acl",        no_argument, &acl_flag,        1},
            {"avu",        no_argument, &avu_flag,        1},
            {"debug",      no_argument, &debug_flag,      1},
            {"help",       no_argument, &help_flag,       1},
            {"timestamp",  no_argument, &timestamp_flag,  1},
            {"unbuffered", no_argument, &unbuffered_flag, 1},
            {"verbose",    no_argument, &verbose_flag,    1},
            {"version",    no_argument, &version_flag,    1},
            // Indexed options
            {"file",      required_argument, NULL, 'f'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long_only(argc, argv, "f:",
                                 long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
            case 'f':
                json_file = optarg;
                break;

            case '?':
                // getopt_long already printed an error message
                break;

            default:
                // Ignore
                break;
        }
    }

    if (acl_flag) pflags       = pflags | PRINT_ACL;
    if (avu_flag) pflags       = pflags | PRINT_AVU;
    if (timestamp_flag) pflags = pflags | PRINT_TIMESTAMP;

    if (help_flag) {
        puts("Name");
        puts("    json-list");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    json-list [--acl] [--avu] [--file <json file>]");
        puts("");
        puts("Description");
        puts("    Lists data objects and collections described in a JSON ");
        puts("    input file.");
        puts("");
        puts("    --acl         Print access control lists in output.");
        puts("    --avu         Print AVU lists in output.");
        puts("    --file        The JSON file describing the data objects and");
        puts("                  collections. Optional, defaults to STDIN.");
        puts("    --timestamp   Print timestamps in output.");
        puts("    --unbuffered  Flush print operations for each JSON object.");
        puts("    --verbose     Print verbose messages to STDERR.");
        puts("");

        exit(0);
    }

    if (version_flag) {
        printf("%s\n", VERSION);
        exit(0);
    }

    if (debug_flag)   set_log_threshold(DEBUG);
    if (verbose_flag) set_log_threshold(NOTICE);

    declare_client_name(argv[0]);
    input = maybe_stdin(json_file);

    int status = do_list_paths(input, pflags);
    if (status != 0) exit_status = 5;

    exit(exit_status);
}

int do_list_paths(FILE *input, print_flags pflags) {
    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    size_t jflags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;

    while (!feof(input)) {
        json_error_t load_error;
        json_t *target = json_loadf(input, jflags, &load_error);
        if (!target) {
            if (!feof(input)) {
                log(ERROR, "JSON error at line %d, column %d: %s",
                    load_error.line, load_error.column, load_error.text);
            }

            continue;
        }

        baton_error_t path_error;
        char *path = json_to_path(target, &path_error);
        path_count++;

        if (path_error.code != 0) {
            error_count++;
            add_error_value(target, &path_error);
            print_json(target);
        }
        else {
            rodsPath_t rods_path;
            int status = resolve_rods_path(conn, &env, &rods_path, path);
            if (status < 0) {
                error_count++;
                set_baton_error(&path_error, status,
                                "Failed to resolve path '%s'", path);
                add_error_value(target, &path_error);
                print_json(target);
            }
            else {
                baton_error_t error;
                json_t *results = list_path(conn, &rods_path, pflags, &error);

                if (error.code != 0) {
                    error_count++;
                    add_error_value(target, &error);
                    print_json(target);
                }
                else {
                    print_json(results);
                    json_decref(results);
                }
            }

            if (rods_path.rodsObjStat) free(rods_path.rodsObjStat);
        }

        if (unbuffered_flag) fflush(stdout);

        json_decref(target);
        free(path);
    } // while

    rcDisconnect(conn);

    log(DEBUG, "Processed %d paths with %d errors", path_count, error_count);

    return error_count;

error:
    if (conn) rcDisconnect(conn);

    log(ERROR, "Processed %d paths with %d errors", path_count, error_count);

    return 1;
}
