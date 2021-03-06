#ifndef AJSON_H__
#define AJSON_H__
#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AJSON_VERSION_CHECK() (ajson_version_major() == AJSON_VERSION_MAJOR && ajson_version_minor() >= AJSON_VERSION_MINOR)

#define AJSON_STACK_SIZE 64 // initial stack size

#define AJSON_FLAG_INTEGER           1 // parse numbers with no "." or exponent as int64_t
#define AJSON_FLAG_NUMBER_COMPONENTS 2 // don't combine numbers into doubles, but return their integer, decimal, and exponent components
#define AJSON_FLAG_NUMBER_AS_STRING  4 // don't convert numbers into doubles, but instead return it as a strings

#define AJSON_FLAGS_NONE 0
#define AJSON_FLAGS_ALL  (AJSON_FLAG_INTEGER | AJSON_FLAG_NUMBER_COMPONENTS | AJSON_FLAG_NUMBER_AS_STRING)

#define AJSON_WRITER_FLAG_ASCII 1 // writer ASCII compatible output (use \u#### escapes)

#define AJSON_WRITER_FLAGS_NONE 0
#define AJSON_WRITER_FLAGS_ALL  AJSON_WRITER_FLAG_ASCII

enum ajson_encoding {
    AJSON_ENC_LATIN1,
    AJSON_ENC_UTF8
};

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
    AJSON_ERROR_PARSER_STATE,
    AJSON_ERROR_PARSER_EXPECTED_DIGIT,
    AJSON_ERROR_PARSER_EXPECTED_HEX,
    AJSON_ERROR_PARSER_EXPECTED_COMMA_OR_ARRAY_END,
    AJSON_ERROR_PARSER_EXPECTED_COMMA_OR_OBJECT_END,
    AJSON_ERROR_PARSER_EXPECTED_STRING,
    AJSON_ERROR_PARSER_EXPECTED_COLON,
    AJSON_ERROR_PARSER_ILLEGAL_ESCAPE,
    AJSON_ERROR_PARSER_ILLEGAL_UNICODE,
    AJSON_ERROR_PARSER_RANGE,
    AJSON_ERROR_PARSER_UNEXPECTED_CHAR,
    AJSON_ERROR_PARSER_UNEXPECTED_EOF
};

struct ajson_parser_s {
    int                 flags;           //!< Parser flags.
    enum ajson_encoding encoding;        //!< Input encoding.
    const char         *input;           //!< Current input chunk.
    size_t              input_size;      //!< Size of current input chunk.
    size_t              input_current;   //!< Index of first not parsed byte in input chunk.
    uintptr_t          *stack;           //!< @private
    size_t              stack_size;      //!< @private
    size_t              stack_current;   //!< @private
    char               *buffer;          //!< @private
    size_t              buffer_size;     //!< @private
    size_t              buffer_used;     //!< @private
    union {
        bool          boolean;           //!< Parsed boolean value.
        double        number;            //!< Parsed number.
        int64_t       integer;           //!< Parsed integer.
        struct {
            bool      positive;          //!< Sign of parsed number.
            bool      exponent_positive; //!< Sign of exponent of parsed number.
            bool      isinteger;         //!< Whether parsed number was an integer.
            uint64_t  integer;           //!< Integer proportion of parsed number.
            uint64_t  decimal;           //!< Decimals of parsed number.
            uint64_t  decimal_places;    //!< Deciaml places of parsed number.
            uint64_t  exponent;          //!< Exponent ot parsed number.
        } components;                    //!< Parsed number components.
        struct {
            uint16_t  unit1;             //!< @private
            uint16_t  unit2;             //!< @private
        } utf16;                         //!< @private
        unsigned char utf8[4];           //!< @private
        struct {
            const char *value;           //!< Parsed string data.
            size_t      length;          //!< Parsed string length.
        } string;                        //!< Parsed string.
        struct {
            enum ajson_error error;      //!< Error code.
            const char      *filename;   //!< C source file where the error occured (currently always "parser.c").
            const char      *function;   //!< C function where the error occured.
            size_t           lineno;     //!< line in C source file where the error occured.
        } error;                         //!< Error information.
    } value; //!< Holds the parsed value or error information.
};

typedef struct ajson_parser_s ajson_parser;

AJSON_EXPORT const char  *ajson_version();
AJSON_EXPORT unsigned int ajson_version_major();
AJSON_EXPORT unsigned int ajson_version_minor();
AJSON_EXPORT unsigned int ajson_version_patch();

AJSON_EXPORT int              ajson_init      (ajson_parser *parser, int flags, enum ajson_encoding encoding);
AJSON_EXPORT void             ajson_reset     (ajson_parser *parser);
AJSON_EXPORT void             ajson_destroy   (ajson_parser *parser);
AJSON_EXPORT int              ajson_feed      (ajson_parser *parser, const void *buffer, size_t size);
AJSON_EXPORT enum ajson_token ajson_next_token(ajson_parser *parser);

AJSON_EXPORT ajson_parser *ajson_alloc(int flags, enum ajson_encoding encoding);
AJSON_EXPORT void          ajson_free (ajson_parser *parser);

AJSON_EXPORT int ajson_get_flags(const ajson_parser *parser);

AJSON_EXPORT bool        ajson_get_boolean      (const ajson_parser *parser);
AJSON_EXPORT double      ajson_get_number       (const ajson_parser *parser);
AJSON_EXPORT int64_t     ajson_get_integer      (const ajson_parser *parser);
AJSON_EXPORT const char* ajson_get_string       (const ajson_parser *parser);
AJSON_EXPORT size_t      ajson_get_string_length(const ajson_parser *parser);

AJSON_EXPORT bool     ajson_get_components_positive         (const ajson_parser *parser);
AJSON_EXPORT bool     ajson_get_components_exponent_positive(const ajson_parser *parser);
AJSON_EXPORT uint64_t ajson_get_components_integer          (const ajson_parser *parser);
AJSON_EXPORT uint64_t ajson_get_components_decimal          (const ajson_parser *parser);
AJSON_EXPORT uint64_t ajson_get_components_decimal_places   (const ajson_parser *parser);
AJSON_EXPORT uint64_t ajson_get_components_exponent         (const ajson_parser *parser);

AJSON_EXPORT enum ajson_error ajson_get_error         (const ajson_parser *parser);
AJSON_EXPORT const char*      ajson_get_error_filename(const ajson_parser *parser);
AJSON_EXPORT const char*      ajson_get_error_function(const ajson_parser *parser);
AJSON_EXPORT size_t           ajson_get_error_lineno  (const ajson_parser *parser);

AJSON_EXPORT const char* ajson_error_str(enum ajson_error error);

typedef int (*ajson_null_func)            (void *ctx);
typedef int (*ajson_boolean_func)         (void *ctx, bool        value);
typedef int (*ajson_number_func)          (void *ctx, double      value);
typedef int (*ajson_number_as_string_func)(void *ctx, const char* value);
typedef int (*ajson_components_func)      (void *ctx, bool positive, uint64_t integer, uint64_t decimal, uint64_t decimal_places, bool exponent_positive, uint64_t exponent);
typedef int (*ajson_integer_func)         (void *ctx, int64_t     value);
typedef int (*ajson_string_func)          (void *ctx, const char* value, size_t length);
typedef int (*ajson_begin_array_func)     (void *ctx);
typedef int (*ajson_end_array_func)       (void *ctx);
typedef int (*ajson_begin_object_func)    (void *ctx);
typedef int (*ajson_end_object_func)      (void *ctx);
typedef int (*ajson_end_func)             (void *ctx);
typedef int (*ajson_error_func)           (void *ctx, enum ajson_error error);

struct ajson_cb_parser_s {
    ajson_parser                parser;                //!< Token parser.
    void                       *ctx;                   //!< Context that will be passed to callbacks.
    ajson_null_func             null_func;             //!< Callback for @c null values.
    ajson_boolean_func          boolean_func;          //!< Callback for boolean values.
    ajson_number_as_string_func number_as_string_func; //!< Callback for numbers when parsed as string.
    ajson_number_func           number_func;           //!< Callback for numbers.
    ajson_components_func       components_func;       //!< Callback for number components.
    ajson_integer_func          integer_func;          //!< Callback for integers.
    ajson_string_func           string_func;           //!< Callback for strings.
    ajson_begin_array_func      begin_array_func;      //!< Callback for array beginnings.
    ajson_end_array_func        end_array_func;        //!< Callback for array ends.
    ajson_begin_object_func     begin_object_func;     //!< Callback for object beginnings.
    ajson_end_object_func       end_object_func;       //!< Callback for object ends.
    ajson_end_func              end_func;              //!< Callback for the end of the JSON document.
    ajson_error_func            error_func;            //!< Callback for errors.
};

typedef struct ajson_cb_parser_s ajson_cb_parser;

AJSON_EXPORT int ajson_cb_parse_fd  (ajson_cb_parser *parser, int fd);
AJSON_EXPORT int ajson_cb_parse_file(ajson_cb_parser *parser, FILE* stream);
AJSON_EXPORT int ajson_cb_parse_buf (ajson_cb_parser *parser, const void* buffer, size_t size);
AJSON_EXPORT int ajson_cb_dispatch  (ajson_cb_parser *parser);

struct ajson_writer_s;

typedef ssize_t (*ajson_write_func)(struct ajson_writer_s *writer, unsigned char *buffer, size_t size, size_t index);

struct ajson_writer_s {
    int              flags;               //!< Writer flags.
    const char      *indent;              //!< Indentation string.
    size_t           nesting_written;     //!< @private
    ajson_write_func write_func;          //!< @private
    ajson_write_func next_write_func;     //!< @private
    uintptr_t        state;               //!< @private
    char            *stack;               //!< @private
    size_t           stack_size;          //!< @private
    size_t           stack_current;       //!< @private
    union {
        const char *string;               //!< @private
        char        character;            //!< @private
    } buffer;                             //!< @private
    union {
        bool boolean;                     //!< @private
        struct {
            double value;                 //!< @private
            size_t written;               //!< @private
            char   buffer[32];            //!< @private
        } number;                         //!< @private
        struct {
            union {
                int64_t  sval;            //!< @private
                uint64_t uval;            //!< @private
            } value;                      //!< @private
            uint64_t div;                 //!< @private
        } integer;                        //!< @private
        struct {
            const char         *value;    //!< @private
            const char         *end;      //!< @private
            enum ajson_encoding encoding; //!< @private
            union {
                size_t       count;       //!< @private
                struct {
                    uint16_t unit1;       //!< @private
                    uint16_t unit2;       //!< @private
                } utf16;                  //!< @private
            } buffer;                     //!< @private
        } string;                         //!< @private
    } value;                              //!< @private
};

typedef struct ajson_writer_s ajson_writer;

AJSON_EXPORT int  ajson_writer_init   (ajson_writer *writer, int flags, const char *indent);
AJSON_EXPORT void ajson_writer_reset  (ajson_writer *writer);
AJSON_EXPORT void ajson_writer_destroy(ajson_writer *writer);

AJSON_EXPORT ajson_writer *ajson_writer_alloc(int flags, const char *indent);
AJSON_EXPORT void          ajson_writer_free (ajson_writer *writer);

AJSON_EXPORT ssize_t ajson_write_null   (ajson_writer *writer, void *buffer, size_t size);
AJSON_EXPORT ssize_t ajson_write_boolean(ajson_writer *writer, void *buffer, size_t size, bool        value);
AJSON_EXPORT ssize_t ajson_write_number (ajson_writer *writer, void *buffer, size_t size, double      value);
AJSON_EXPORT ssize_t ajson_write_integer(ajson_writer *writer, void *buffer, size_t size, int64_t     value);
AJSON_EXPORT ssize_t ajson_write_string (ajson_writer *writer, void *buffer, size_t size, const char* value, size_t length, enum ajson_encoding encoding);
AJSON_EXPORT ssize_t ajson_write_string_latin1(ajson_writer *writer, void *buffer, size_t size, const char* value);
AJSON_EXPORT ssize_t ajson_write_string_utf8  (ajson_writer *writer, void *buffer, size_t size, const char* value);

AJSON_EXPORT ssize_t ajson_write_begin_array(ajson_writer *writer, void *buffer, size_t size);
AJSON_EXPORT ssize_t ajson_write_end_array  (ajson_writer *writer, void *buffer, size_t size);

AJSON_EXPORT ssize_t ajson_write_begin_object(ajson_writer *writer, void *buffer, size_t size);
AJSON_EXPORT ssize_t ajson_write_end_object  (ajson_writer *writer, void *buffer, size_t size);

AJSON_EXPORT ssize_t ajson_write_continue(ajson_writer *writer, void *buffer, size_t size);

AJSON_EXPORT int         ajson_writer_get_flags (ajson_writer *writer);
AJSON_EXPORT const char *ajson_writer_get_indent(ajson_writer *writer);

AJSON_EXPORT int ajson_decode_utf8(const unsigned char buffer[], size_t size, uint32_t *codepoint);

#ifdef __cplusplus
}
#endif

#endif
