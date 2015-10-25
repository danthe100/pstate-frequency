/**
 * @file psfreq_strings.c
 * @author pyamsoft <pyam(dot)soft(at)gmail(dot)com>
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * @section DESCRIPTION
 * A couple of general purpose functions which can be applied to various
 * character array string like structures in C.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psfreq_color.h"
#include "psfreq_log.h"
#include "psfreq_strings.h"

/**
 * Concatinate two strings together
 *
 * @param s1 Base string that will be concatinated upon
 * @param s2 String that will be appended to s1
 * @return String which is the combination of s1 and s2
 */
char *psfreq_strings_concat(const char *const s1, const char *const s2)
{
        char *result = NULL;
        if (asprintf(&result, "%s%s", s1, s2) < 0) {
                psfreq_log_error("psfreq_strings_concat",
                        "asprintf returned a -1, indicating a failure during\n"
                        "either memory allocation or some other error.");
                return NULL;
        }
        return result;
}

unsigned char psfreq_strings_starts_with(const char *s, const char *p)
{
        psfreq_log_debug("psfreq_strings_starts_with",
                        "Check if string '%s' starts with '%s'", s, p);
        while (*p) {
                if (*p++ != *s++) {
                        return 0;
                }
        }
        return 1;
}

unsigned char psfreq_strings_isdigits(const char *s)
{
        while (*s) {
                if (isdigit(*s++) == 0) {
                        return 0;
                }
        }
        return 1;
}

unsigned char psfreq_strings_equals(const char *s, const char *p)
{
        const size_t size = strlen(s);
        const size_t ssize = strlen(p);
        if (size != ssize) {
                psfreq_log_debug("psfreq_strings_equals",
                                "Different size strings");
                return 0;
        }
        psfreq_log_debug("psfreq_strings_equals",
                        "Check if string '%s' is '%s'", s, p);

        return (strncmp(s, p, size) == 0);
}

double psfreq_strings_to_double(const char *const s)
{
        const double v = strtol(s, NULL, RADIX_DECIMAL);
        psfreq_log_debug("psfreq_strings_to_double",
                        "Convert string '%s' to double value", s);
        return v;
}

int psfreq_strings_to_int(const char *const s)
{
        return (int) psfreq_strings_to_double(s);
}

unsigned int psfreq_strings_to_uint(const char *const s)
{
        return (unsigned int) psfreq_strings_to_double(s);
}

char *psfreq_strings_from_double(const double *const d)
{
        char *buf = NULL;
        if (asprintf(&buf, "%f", *d) < 0) {
                psfreq_log_error("psfreq_strings_from_double",
                        "asprintf returned a -1, indicating a failure during\n"
                        "either memory allocation or some other error.");
                return NULL;
        }
        return buf;
}

char *psfreq_strings_from_int(const int *const i)
{
        char *buf = NULL;
        if (asprintf(&buf, "%d", *i) < 0) {
                psfreq_log_error("psfreq_strings_from_int",
                        "asprintf returned a -1, indicating a failure during\n"
                        "either memory allocation or some other error.");
                return NULL;
        }
        return buf;
}

char *psfreq_strings_from_uint(const unsigned int *const u)
{
        char *buf = NULL;
        if (asprintf(&buf, "%u", *u) < 0) {
                psfreq_log_error("psfreq_strings_from_int",
                        "asprintf returned a -1, indicating a failure during\n"
                        "either memory allocation or some other error.");
                return NULL;
        }
        return buf;
}

char *psfreq_strings_strip_end(char *s)
{
        char *ns = NULL;
        unsigned int i;
        psfreq_log_debug("psfreq_strings_strip_end",
                        "Strip newline from string: '%s'", s);
        if (s == NULL) {
                psfreq_log_error("psfreq_strings_strip_end",
                                "String is NULL");
                return NULL;
        }
        i = strlen(s) - 1;
        if ((i > 0) && (s[i] == '\n')) {
                s[i] = '\0';
        }
        psfreq_log_debug("psfreq_strings_strip_end",
                        "Stripped string is: '%s'", s);
        if (asprintf(&ns, "%s", s) < 0) {
                psfreq_log_error("psfreq_strings_strip_end",
                        "asprintf returned a -1, indicating a failure during\n"
                        "either memory allocation or some other error.");
                return NULL;
        }
        return ns;
}

char **psfreq_strings_strtok(char *s, const size_t num)
{
        char** arr = malloc(num * sizeof(char *));
        size_t i = 0;
        const char *const del = " ,.-";
        char *tok;
        char *saveptr;
        psfreq_log_debug("psfreq_strings_strtok",
                        "Split string '%s' by delims '%s'", s, del);
        tok = strtok_r(s, del, &saveptr);
        psfreq_log_debug("psfreq_strings_strtok", "First tok success");
        while (tok != NULL) {
                psfreq_log_debug("psfreq_strings_strok",
                                "assign '%s' to arr[%d]", tok, i);
                arr[i++] = tok;
                tok = strtok_r(NULL, del, &saveptr);
        }
        return arr;
}

