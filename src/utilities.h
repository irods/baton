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
 * @file utilities.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_UTILITIES_H
#define _BATON_UTILITIES_H

#include <stdio.h>

#define ISO8601_FORMAT "%Y-%m-%dT%H:%M:%S"

int str_starts_with(const char *str, const char *prefix);

int str_ends_with(const char *str, const char *suffix);

int str_equals(const char *str1, const char *str2);

int str_equals_ignore_case(const char *str1, const char *str2);

char *copy_str(const char *str);

const char *parse_base_name(const char *path);

FILE *maybe_stdin(const char *path);

char *format_timestamp(const char *timestamp, const char *format);

char *parse_timestamp(const char *timestamp, const char *format);

#endif // _BATON_UTILITIES_H
