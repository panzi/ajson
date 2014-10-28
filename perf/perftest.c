/*
 * Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
 *                    2014, Mathias Panzenböck <grosser.meister.morti@gmx.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * The performance test code is copied from yajl (which is much faster than this):
 * http://lloyd.github.io/yajl/
 */

#include "ajson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "documents.h"

/* a platform specific defn' of a function to get a high res time in a
 * portable format */
#ifndef WIN32
#include <sys/time.h>
static double mygettime(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + (now.tv_usec / 1000000.0);
}
#else
#define _WIN32 1
#include <windows.h>
static double mygettime(void) {
    long long tval;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    tval = ft.dwHighDateTime;
    tval <<=32;
    tval |= ft.dwLowDateTime;
    return tval / 10000000.00;
}
#endif

#define PARSE_TIME_SECS 3

static int
parse_doc(ajson_parser *parser, const char **doc)
{
    ajson_clear(parser);
    if (ajson_feed(parser, *doc, strlen(*doc)) != 0) {
        return 1;
    }

    ++ doc;

    for (;;) {
        enum ajson_token token = ajson_next_token(parser);

        switch (token) {
        case AJSON_TOK_ERROR:
            return 2;

        case AJSON_TOK_NEED_DATA:
            if (*doc) {
                if (ajson_feed(parser, *doc, strlen(*doc)) != 0) {
                    return 1;
                }
                ++ doc;
            }
            else {
                // signal that I don't have any more data
                ajson_feed(parser, "", 0);
            }
            break;

        case AJSON_TOK_END:
            return 0;

        default:
            break;
        }
    }
}

static int
run(int flags)
{
    long long times = 0; 
    double starttime;

    starttime = mygettime();

    /* allocate a parser */
    for (;;) {
        int i;
        {
            double now = mygettime();
            if (now - starttime >= PARSE_TIME_SECS) break;
        }

        for (i = 0; i < 100; i++) {
            ajson_parser parser;
            if (ajson_init(&parser, flags, AJSON_ENC_UTF8) != 0) {
                return 1;
            }
            int stat = parse_doc(&parser, get_doc(times % num_docs()));

            if (stat != 0) {
                if (stat == 2) {
                    fprintf(stderr, "parsing document %d: %s\n", i, ajson_error_str(parser.value.error.error));
                    fprintf(stderr, "%s:%zu: %s: error raised here\n", parser.value.error.filename, parser.value.error.lineno, parser.value.error.function);
                }
                else {
                    fprintf(stderr, "parsing document %d: %s\n", i, strerror(errno));
                }
                ajson_destroy(&parser);
                return 1;
            }
            ajson_destroy(&parser);
            times++;
        }
    }

    /* parsed doc 'times' times */
    {
        double throughput;
        double now;
        const char * all_units[] = { "B/s", "KB/s", "MB/s", (char *) 0 };
        const char ** units = all_units;
        int i, avg_doc_size = 0;

        now = mygettime();

        for (i = 0; i < num_docs(); i++) avg_doc_size += doc_size(i);
        avg_doc_size /= num_docs();

        throughput = (times * avg_doc_size) / (now - starttime);
        
        while (*(units + 1) && throughput > 1024) {
            throughput /= 1024;
            units++;
        }

        printf("Parsing speed: %g %s\n", throughput, *units);
    }

    return 0;
}

int
main(void)
{
    int rv = 0;

    printf("-- speed tests determine parsing throughput given %d different sample documents --\n",
           num_docs());

    printf("Parsing all numbers as double:\n");
    rv = run(AJSON_FLAGS_NONE);
    if (rv != 0) return rv;
    
    printf("Parsing integers as int64_t:\n");
    rv = run(AJSON_FLAG_INTEGER);
    if (rv != 0) return rv;
    
    printf("Parsing numbers as strings:\n");
    rv = run(AJSON_FLAG_INTEGER);
    if (rv != 0) return rv;

    return 0;
}

