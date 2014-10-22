#include "ajson.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

size_t ajson_get_lineno  (const ajson_parser *parser) { return parser->lineno;   }
size_t ajson_get_columnno(const ajson_parser *parser) { return parser->columnno; }

bool        ajson_get_boolean(const ajson_parser *parser) { return parser->value.boolean; }
double      ajson_get_number (const ajson_parser *parser) { return parser->value.number;  }
int64_t     ajson_get_integer(const ajson_parser *parser) { return parser->value.integer; }
const char* ajson_get_string (const ajson_parser *parser) { return parser->value.string;  }

bool     ajson_get_components_positive         (const ajson_parser *parser) { return parser->value.components.positive;          }
bool     ajson_get_components_exponent_positive(const ajson_parser *parser) { return parser->value.components.exponent_positive; }
uint64_t ajson_get_components_integer          (const ajson_parser *parser) { return parser->value.components.integer;           }
uint64_t ajson_get_components_decimal          (const ajson_parser *parser) { return parser->value.components.decimal;           }
uint64_t ajson_get_components_decimal_places   (const ajson_parser *parser) { return parser->value.components.decimal_places;    }
uint64_t ajson_get_components_exponent         (const ajson_parser *parser) { return parser->value.components.exponent;          }

enum ajson_error ajson_get_error         (const ajson_parser *parser) { return parser->value.error.error;    }
const char*      ajson_get_error_filename(const ajson_parser *parser) { return parser->value.error.filename; }
const char*      ajson_get_error_function(const ajson_parser *parser) { return parser->value.error.function; }
size_t           ajson_get_error_lineno  (const ajson_parser *parser) { return parser->value.error.lineno;   }

const char* ajson_error_str(enum ajson_error error) {
    switch (error) {
    case AJSON_ERROR_EMPTY_SATCK:
        return "empty stack";

    case AJSON_ERROR_JUMP:
        return "illegal jump";

    case AJSON_ERROR_MEMORY:
        return "out of memory";

    case AJSON_ERROR_PARSER:
        return "parser error";

    case AJSON_ERROR_PARSER_UNEXPECTED:
        return "unexpected character";

    case AJSON_ERROR_PARSER_EXPECTED_ARRAY_END:
        return "expected ]";

    case AJSON_ERROR_PARSER_EXPECTED_OBJECT_END:
        return "expected }";

    case AJSON_ERROR_PARSER_UNICODE:
        return "illegal unicode codepoint";

    case AJSON_ERROR_PARSER_UNEXPECTED_EOF:
        return "unexpected end of file";

    case AJSON_ERROR_NONE:
        return "no error";

    default:
        return "unknown error";
    }
}

int ajson_init(ajson_parser *parser, int flags) {
    if (flags & ~AJSON_FLAG_INTEGER) {
        errno = EINVAL;
        return -1;
    }

    memset(parser, 0, sizeof(ajson_parser));
    parser->lineno = 1;
    parser->flags  = flags;

    parser->stack = calloc(AJSON_STACK_SIZE, sizeof(uintptr_t));
    if (!parser->stack) {
        return -1;
    }
    parser->stack_size = AJSON_STACK_SIZE;

    return 0;
}

void ajson_destroy(ajson_parser *parser) {
    free(parser->buffer);
    parser->buffer      = NULL;
    parser->buffer_size = 0;
    parser->buffer_used = 0;

    free(parser->stack);
    parser->stack         = NULL;
    parser->stack_size    = 0;
    parser->stack_current = 0;
}

int ajson_feed(ajson_parser *parser, const void *buffer, size_t size) {
    if (parser->input_current < parser->input_size) {
        errno = ENOBUFS;
        return -1;
    }
    parser->input         = buffer;
    parser->input_size    = size;
    parser->input_current = 0;
    return 0;
}

ajson_parser *ajson_alloc(int flags) {
    ajson_parser *parser = malloc(sizeof(ajson_parser));

    if (parser) {
        if (ajson_init(parser, flags) != 0) {
            free(parser);
            return NULL;
        }
    }

    return parser;
}

void ajson_free(ajson_parser *parser) {
    if (parser) {
        ajson_destroy(parser);
        free(parser);
    }
}

