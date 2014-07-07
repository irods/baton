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
static int help_flag;
static int unbuffered_flag;
static int verbose_flag;
static int version_flag;

int do_list_metadata(FILE *input);

int main(int argc, char *argv[]) {
    int exit_status = 0;
    char *json_file = NULL;
    FILE *input = NULL;

    while (1) {
        static struct option long_options[] = {
            // Flag options
            {"debug",      no_argument, &debug_flag,      1},
            {"help",       no_argument, &help_flag,       1},
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

    if (help_flag) {
        puts("Name");
        puts("    json-metalist");
        puts("");
        puts("Synopsis");
        puts("");
        puts("    json-metalist [--file <JSON file>]");
        puts("");
        puts("Description");
        puts("    Lists metadata AVUs on data objects and collections");
        puts("described in a JSON input file.");
        puts("");
        puts("    --file        The JSON file describing the data objects and");
        puts("                  collections. Optional, defaults to STDIN.");
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

    int status = do_list_metadata(input);
    if (status != 0) exit_status = 5;

    exit(exit_status);
}

int do_list_metadata(FILE *input) {
    int path_count = 0;
    int error_count = 0;

    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    if (!conn) goto error;

    size_t flags = JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES;

    while (!feof(input)) {
        json_error_t load_error;
        json_t *target = json_loadf(input, flags, &load_error);
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
            log(ERROR, "Failed to convert path '%s' to JSON", path);
            error_count++;
            log(ERROR, "Failed to convert path '%s' to JSON", path);

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
                json_t *avus = list_metadata(conn, &rods_path, NULL, &error);

                if (error.code != 0) {
                    error_count++;
                    add_error_value(target, &error);
                    print_json(target);
                }
                else {
                    log(DEBUG, "Listed metadata on '%s'", path);
                    json_object_set_new(target, JSON_AVUS_KEY, avus);

                    char *str = json_dumps(target, JSON_INDENT(0));
                    log(DEBUG, "Sending JSON: %s", str);
                    free(str);

                    print_json(target);
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
