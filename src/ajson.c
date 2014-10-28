#include "ajson.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

const char  *ajson_version()       { return AJSON_VERSION_STRING; }
unsigned int ajson_version_major() { return AJSON_VERSION_MAJOR; }
unsigned int ajson_version_minor() { return AJSON_VERSION_MINOR; }
unsigned int ajson_version_patch() { return AJSON_VERSION_PATCH; }

int ajson_get_flags(const ajson_parser *parser) { return parser->flags; }

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
        return "parser in error state";

    case AJSON_ERROR_PARSER_UNEXPECTED:
        return "unexpected character";

    case AJSON_ERROR_PARSER_EXPECTED_ARRAY_END:
        return "expected ]";

    case AJSON_ERROR_PARSER_EXPECTED_OBJECT_END:
        return "expected }";

    case AJSON_ERROR_PARSER_UNICODE:
        return "illegal unicode codepoint";

    case AJSON_ERROR_PARSER_RANGE:
        return "numeric value out of range";

    case AJSON_ERROR_PARSER_UNEXPECTED_EOF:
        return "unexpected end of file";

    case AJSON_ERROR_NONE:
        return "no error";

    default:
        return "unknown error";
    }
}

int ajson_init(ajson_parser *parser, int flags, enum ajson_encoding encoding) {
    if (flags & ~AJSON_FLAGS_ALL || (flags & AJSON_FLAG_NUMBER_AS_STRING && flags & (AJSON_FLAG_INTEGER | AJSON_FLAG_NUMBER_COMPONENTS))) {
        errno = EINVAL;
        return -1;
    }

    memset(parser, 0, sizeof(ajson_parser));
    parser->flags    = flags;
    parser->encoding = encoding;

    parser->stack = calloc(AJSON_STACK_SIZE, sizeof(uintptr_t));
    if (!parser->stack) {
        return -1;
    }
    parser->stack_size = AJSON_STACK_SIZE;

    return 0;
}

void ajson_clear(ajson_parser *parser) {
    if (parser->stack) parser->stack[0] = 0;
    parser->stack_current = 0;
    parser->buffer_used   = 0;
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

ajson_parser *ajson_alloc(int flags, enum ajson_encoding encoding) {
    ajson_parser *parser = malloc(sizeof(ajson_parser));

    if (parser) {
        if (ajson_init(parser, flags, encoding) != 0) {
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

int ajson_decode_utf8(const unsigned char buffer[], size_t size, uint32_t *codepoint) {
    if (size == 0) {
        errno = EINVAL;
        return -1;
    }

    unsigned char unit1 = buffer[0];
    unsigned char unit2, unit3, unit4;

    if (unit1 < 0x80) {
        // single byte
        if (codepoint) {
            *codepoint = unit1;
        }
        return 1;
    }
    else if (unit1 < 0xC2) {
        // unexpected continuation or overlong 2-byte sequence
    }
    else if (unit1 < 0xE0) {
        // 2-byte sequence
        if (size < 2) {
            errno = EINVAL;
            return -2;
        }

        unit2 = buffer[1];
        if ((unit2 & 0xC0) != 0x80) {
            errno = EILSEQ;
            return -2;
        }

        if (codepoint) {
            *codepoint = (unit1 << 6) + unit2 - 0x3080;
        }

        return 2;
    }
    else if (unit1 < 0xF0) {
        // 3-byte sequence
        if (size < 3) {
            errno = EINVAL;
            return -3;
        }

        unit2 = buffer[1];
        unit3 = buffer[2];

        if ((unit2 & 0xC0) != 0x80 || (unit1 == 0xE0 && unit2 < 0xA0)) {
            errno = EILSEQ;
            return -2;
        }

        if ((unit3 & 0xC0) != 0x80) {
            errno = EILSEQ;
            return -3;
        }

        if (codepoint) {
            *codepoint = (unit1 << 12) + (unit2 << 6) + unit3 - 0xE2080;
        }

        return 3;
    }
    else if (unit1 < 0xF5) {
        // 4-byte sequence
        if (size < 4) {
            errno = EINVAL;
            return -4;
        }

        unit2 = buffer[1];
        unit3 = buffer[2];
        unit4 = buffer[3];

        if ((unit2 & 0xC0) != 0x80 || (unit1 == 0xF0 && unit2 < 0x90) || (unit1 == 0xF4 && unit2 >= 0x90)) {
            errno = EILSEQ;
            return -2;
        }

        if ((unit3 & 0xC0) != 0x80) {
            errno = EILSEQ;
            return -3;
        }

        if ((unit4 & 0xC0) != 0x80) {
            errno = EILSEQ;
            return -4;
        }

        if (codepoint) {
            *codepoint = (unit1 << 18) + (unit2 << 12) + (unit3 << 6) + unit4 - 0x3C82080;
        }

        return 4;
    }

    errno = EILSEQ;
    return -1;
}
