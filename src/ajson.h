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

#define AJSON_STACK_SIZE 64

#define AJSON_FLAG_NONE              0
#define AJSON_FLAG_INTEGER           1 // parse numbers with no "." as int64_t
#define AJSON_FLAG_NUMBER_COMPONENTS 2 // don't combine numbers into doubles but return their integer, decimal, and exponent components

enum ajson_token {
    AJSON_TOK_NEED_DATA,
    AJSON_TOK_NULL,
    AJSON_TOK_BOOLEAN,
    AJSON_TOK_NUMBER,
    AJSON_TOK_INTEGER,
    AJSON_TOK_STRING,
    AJSON_TOK_BEGIN_ARRAY,
    AJSON_TOK_END_ARRAY,
    AJSON_TOK_BEGIN_OBJECT,
    AJSON_TOK_END_OBJECT,
    AJSON_TOK_END,
    AJSON_TOK_ERROR
};

enum ajson_error {
    AJSON_ERROR_NONE,
    AJSON_ERROR_MEMORY,
    AJSON_ERROR_EMPTY_SATCK, // internal error
    AJSON_ERROR_JUMP,        // internal error
    AJSON_ERROR_PARSER,
    AJSON_ERROR_PARSER_UNEXPECTED,
    AJSON_ERROR_PARSER_EXPECTED_ARRAY_END,
    AJSON_ERROR_PARSER_EXPECTED_OBJECT_END,
    AJSON_ERROR_PARSER_UNICODE,
    AJSON_ERROR_PARSER_RANGE,
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
    char        *buffer;
    size_t       buffer_size;
    size_t       buffer_used;
    union {
        bool          boolean;
        double        number;
        int64_t       integer;
        struct {
            bool      positive;
            bool      exponent_positive;
            bool      isinteger;
            uint64_t  integer;
            uint64_t  decimal;
            uint64_t  decimal_places;
            uint64_t  exponent;
        } components;
        struct {
            uint16_t  unit1;
            uint16_t  unit2;
        } surrogate_pair;
        const char   *string;
        struct {
            enum ajson_error error;
            const char      *filename;
            const char      *function;
            size_t           lineno;
        } error;
    } value;
};

typedef struct ajson_parser_s ajson_parser;

int              ajson_init      (ajson_parser *parser, int flags);
void             ajson_destroy   (ajson_parser *parser);
int              ajson_feed      (ajson_parser *parser, const void *buffer, size_t size);
enum ajson_token ajson_next_token(ajson_parser *parser);

ajson_parser *ajson_alloc(int flags);
void          ajson_free (ajson_parser *parser);

size_t ajson_get_lineno  (const ajson_parser *parser);
size_t ajson_get_columnno(const ajson_parser *parser);

bool        ajson_get_boolean(const ajson_parser *parser);
double      ajson_get_number (const ajson_parser *parser);
int64_t     ajson_get_integer(const ajson_parser *parser);
const char* ajson_get_string (const ajson_parser *parser);

bool     ajson_get_components_positive         (const ajson_parser *parser);
bool     ajson_get_components_exponent_positive(const ajson_parser *parser);
uint64_t ajson_get_components_integer          (const ajson_parser *parser);
uint64_t ajson_get_components_decimal          (const ajson_parser *parser);
uint64_t ajson_get_components_decimal_places   (const ajson_parser *parser);
uint64_t ajson_get_components_exponent         (const ajson_parser *parser);

enum ajson_error ajson_get_error         (const ajson_parser *parser);
const char*      ajson_get_error_filename(const ajson_parser *parser);
const char*      ajson_get_error_function(const ajson_parser *parser);
size_t           ajson_get_error_lineno  (const ajson_parser *parser);

const char* ajson_error_str(enum ajson_error error);

typedef int (*ajson_null_func)        (void *ctx);
typedef int (*ajson_boolean_func)     (void *ctx, bool        value);
typedef int (*ajson_number_func)      (void *ctx, double      value);
typedef int (*ajson_components_func)  (void *ctx, bool positive, uint64_t integer, uint64_t decimal, uint64_t decimal_places, bool exponent_positive, uint64_t exponent);
typedef int (*ajson_integer_func)     (void *ctx, int64_t     value);
typedef int (*ajson_string_func)      (void *ctx, const char* value);
typedef int (*ajson_begin_array_func) (void *ctx);
typedef int (*ajson_end_array_func)   (void *ctx);
typedef int (*ajson_begin_object_func)(void *ctx);
typedef int (*ajson_end_object_func)  (void *ctx);
typedef int (*ajson_end_func)         (void *ctx);
typedef int (*ajson_error_func)       (void *ctx, enum ajson_error error);

struct ajson_cb_parser_s {
    ajson_parser            parser;
    void                   *ctx;
    ajson_null_func         null_func;
    ajson_boolean_func      boolean_func;
    ajson_number_func       number_func;
    ajson_components_func   components_func;
    ajson_integer_func      integer_func;
    ajson_string_func       string_func;
    ajson_begin_array_func  begin_array_func;
    ajson_end_array_func    end_array_func;
    ajson_begin_object_func begin_object_func;
    ajson_end_object_func   end_object_func;
    ajson_end_func          end_func;
    ajson_error_func        error_func;
};

typedef struct ajson_cb_parser_s ajson_cb_parser;

int ajson_cb_parse_fd  (ajson_cb_parser *parser, int fd);
int ajson_cb_parse_file(ajson_cb_parser *parser, FILE* stream);
int ajson_cb_parse_buf (ajson_cb_parser *parser, const void* buffer, size_t size);
int ajson_cb_dispatch  (ajson_cb_parser *parser);

#ifdef __cplusplus
}
#endif

#endif
