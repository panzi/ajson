#ifndef AJSON_H__
#define AJSON_H__
#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AJSON_FLAG_NONE    0
#define AJSON_FLAG_INTEGER 1 // parse numbers with no "." as long int

enum ajson_token {
    AJSON_TOK_EOF,
    AJSON_TOK_NULL,
    AJSON_TOK_BOOLEAN,
    AJSON_TOK_NUMBER,
    AJSON_TOK_INTEGER,
    AJSON_TOK_STRING,
    AJSON_TOK_BEGIN_ARRAY,
    AJSON_TOK_END_ARRAY,
    AJSON_TOK_BEGIN_OBJECT,
    AJSON_TOK_OBJECT_KEY,
    AJSON_TOK_END_OBJECT,
    AJSON_TOK_ERROR
};

enum ajson_error {
    AJSON_ERROR_NONE,
    AJSON_ERROR_MEMORY,
    AJSON_ERROR_EMPTY_SATCK, // internal error
    AJSON_ERROR_PARSER,
    AJSON_ERROR_PARSER_UNEXPECTED,
    AJSON_ERROR_PARSER_EXPECTED_ARRAY_END,
    AJSON_ERROR_PARSER_EXPECTED_OBJECT_END,
    AJSON_ERROR_PARSER_UNICODE,
    AJSON_ERROR_PARSER_UNEXPECTED_EOF
};

struct ajson_parser_s {
    int          flags;
    const char  *input;
    size_t       input_size;
    size_t       input_current;
    size_t       lineno;
    size_t       columnno;
    uintptr_t   *stack;
    size_t       stack_size;
    size_t       stack_current;
    char  *buffer;
    size_t buffer_size;
    size_t buffer_used;
    enum ajson_token token;
    union {
        bool             boolean;
        double           number;
        long             integer;
        const char      *string;
        enum ajson_error error;
    } value;
};

typedef struct ajson_parser_s ajson_parser;

int              ajson_init      (ajson_parser *parser, int flags);
void             ajson_destroy   (ajson_parser *parser);
int              ajson_feed      (ajson_parser *parser, const void *buffer, size_t size);
enum ajson_token ajson_next_token(ajson_parser *parser);

const char* ajson_error_str(enum ajson_error error);

typedef int (*ajson_null_func)        (void *ctx);
typedef int (*ajson_boolean_func)     (void *ctx, bool        value);
typedef int (*ajson_number_func)      (void *ctx, double      value);
typedef int (*ajson_integer_func)     (void *ctx, long        value);
typedef int (*ajson_string_func)      (void *ctx, const char* value);
typedef int (*ajson_begin_array_func) (void *ctx);
typedef int (*ajson_end_array_func)   (void *ctx);
typedef int (*ajson_begin_object_func)(void *ctx);
typedef int (*ajson_object_key_func)  (void *ctx, const char* key);
typedef int (*ajson_end_object_func)  (void *ctx);
typedef int (*ajson_error_func)       (void *ctx, int errnum);

struct ajson_cb_parser_s {
    ajson_parser            parser;
    void                   *ctx;
    ajson_null_func         null_func;
    ajson_boolean_func      boolean_func;
    ajson_number_func       number_func;
    ajson_integer_func      integer_func;
    ajson_string_func       string_func;
    ajson_begin_array_func  begin_array_func;
    ajson_end_array_func    end_array_func;
    ajson_begin_object_func begin_object_func;
    ajson_object_key_func   object_key_func;
    ajson_end_object_func   end_object_func;
    ajson_error_func        error_func;
};

typedef struct ajson_cb_parser_s ajson_cb_parser;

int ajson_cb_feed_fd  (ajson_cb_parser *parser, int fd);
int ajson_cb_feed_file(ajson_cb_parser *parser, FILE* stream);
int ajson_cb_feed_buf (ajson_cb_parser *parser, const void* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
