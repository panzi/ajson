#include "ajson.h"

#include <inttypes.h>
#include <ctype.h>
#include <math.h>

#define AJSON_SET_ERROR(PARSER, ERR) \
    (PARSER)->value.error.error    = (ERR); \
    (PARSER)->value.error.filename = __FILE__; \
    (PARSER)->value.error.function = __func__; \
    (PARSER)->value.error.lineno   = __LINE__;

static inline int ajson_push(ajson_parser *parser, uintptr_t state) {
    if (parser->stack_current + 1 == parser->buffer_size) {
        size_t newsize = parser->stack_size + AJSON_STACK_SIZE;
        uintptr_t *stack = SIZE_MAX/sizeof(uintptr_t) < newsize ? NULL : realloc(parser->stack, sizeof(uintptr_t) * newsize);
        if (stack == NULL) {
            AJSON_SET_ERROR(parser, AJSON_ERROR_MEMORY);
            return -1;
        }
        parser->stack = stack;
        parser->stack_size = newsize;
    }
    parser->stack[parser->stack_current ++] = state;
    return 0;
}

static inline int ajson_buffer_ensure(ajson_parser *parser, size_t space) {
    size_t needed = parser->buffer_used + space;
    if (parser->buffer_size < needed) {
        size_t newsize = needed - (needed % BUFSIZ);
        if (newsize < needed) {
            newsize += BUFSIZ;
        }
        char *buffer = realloc(parser->buffer, newsize);
        if (buffer == NULL) {
            AJSON_SET_ERROR(parser, AJSON_ERROR_MEMORY);
            return -1;
        }
        parser->buffer = buffer;
        parser->buffer_size = newsize;
    }
    return 0;
}

static inline int ajson_buffer_putc(ajson_parser *parser, char ch) {
    if (parser->buffer_size == parser->buffer_used) {
        size_t newsize = parser->buffer_size + BUFSIZ;
        char *buffer = realloc(parser->buffer, newsize);
        if (buffer == NULL) {
            AJSON_SET_ERROR(parser, AJSON_ERROR_MEMORY);
            return -1;
        }
        parser->buffer = buffer;
        parser->buffer_size = newsize;
    }
    parser->buffer[parser->buffer_used ++] = ch;
    return 0;
}

static inline int ajson_buffer_putcp(ajson_parser *parser, uint32_t codepoint) {
    if (codepoint < 0x80) {
        // 0xxxxxxx
        if (ajson_buffer_ensure(parser, 1) != 0) {
            return -1;
        }
        parser->buffer[parser->buffer_used ++] =  codepoint;
    }
    else if (codepoint <= 0x7FF) {
        // 110xxxxx 10xxxxxx
        if (ajson_buffer_ensure(parser, 2) != 0) {
            return -1;
        }
        parser->buffer[parser->buffer_used ++] = (codepoint >> 6) + 0xC0;
        parser->buffer[parser->buffer_used ++] = (codepoint & 0x3F) + 0x80;
    }
    else if (codepoint <= 0xFFFF) {
        // 1110xxxx 10xxxxxx 10xxxxxx
        if (ajson_buffer_ensure(parser, 3) != 0) {
            return -1;
        }
        parser->buffer[parser->buffer_used ++] = (codepoint >> 12) + 0xE0;
        parser->buffer[parser->buffer_used ++] = ((codepoint >> 6) & 0x3F) + 0x80;
        parser->buffer[parser->buffer_used ++] = (codepoint & 0x3F) + 0x80;
    }
    else if (codepoint <= 0x10FFFF) {
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if (ajson_buffer_ensure(parser, 4) != 0) {
            return -1;
        }
        parser->buffer[parser->buffer_used ++] = (codepoint >> 18) + 0xF0;
        parser->buffer[parser->buffer_used ++] = ((codepoint >> 12) & 0x3F) + 0x80;
        parser->buffer[parser->buffer_used ++] = ((codepoint >> 6) & 0x3F) + 0x80;
        parser->buffer[parser->buffer_used ++] = (codepoint & 0x3F) + 0x80;
    }
    else {
        AJSON_SET_ERROR(parser, AJSON_ERROR_PARSER_UNICODE);
        return -1;
    }
    return 0;
}

static inline void ajson_buffer_clear(ajson_parser *parser) {
    parser->buffer_used = 0;
}

#ifdef __GNUC__
#   define AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#endif

#define DISPATCH_PRELUDE \
    const char*  input = parser->input; \
    size_t       index = parser->input_current; \
    const size_t size  = parser->input_size;

#ifdef AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#pragma GCC diagnostic ignored "-pedantic"
#pragma GCC diagnostic ignored "-Wunused-label"
    // http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
    // use offsets instead of absolute addresses to reduce the number of
    // dynamic relocations for code in shared libraries

#   define AUTO_STATE_NAME2(LINE) do_ ## LINE
#   define AUTO_STATE_NAME(LINE) AUTO_STATE_NAME2(LINE)
#   define STATE_REF(NAME) (&&do_named_ ## NAME - &&do_named_START)
#   define STATE(NAME) do_named_ ## NAME :
#   define AUTO_STATE_REF() (&&AUTO_STATE_NAME(__LINE__) - &&do_named_START)
#   define AUTO_STATE() AUTO_STATE_NAME(__LINE__):
#   define BEGIN_DISPATCH DISPATCH_PRELUDE; goto *(&&do_named_START + parser->stack[parser->stack_current]);
#   define END_DISPATCH
#   define DISPATCH(NAME) goto do_named_ ## NAME;

#else

enum ajson_named_states {
    AJSON_STATE_START,
    AJSON_STATE_VALUE,
    AJSON_STATE_STRING,
    AJSON_STATE_ERROR,

    AJSON_STATECOUNT
};

#   define STATE_REF(NAME) AJSON_STATE_ ## NAME
#   define STATE(NAME) case STATE_REF(NAME):
#   define AUTO_STATE_REF() (AJSON_STATECOUNT + __LINE__)
#   define AUTO_STATE() case AJSON_STATECOUNT + __LINE__:
#   define BEGIN_DISPATCH DISPATCH_PRELUDE; uintptr_t state = parser->stack[parser->stack_current]; dispatch_loop: switch (state) {
#   define END_DISPATCH } RAISE_ERROR(AJSON_ERROR_JUMP);
#   define DISPATCH(NAME) state = AJSON_STATE_ ## NAME; goto dispatch_loop;

#endif

#define AT_EOF() (size == 0)
#define CURR_CH() (input[index])

#define RQUIRE_DATA(JUMP) \
    if (index >= size) { \
        parser->input_current = index; \
        parser->stack[parser->stack_current] = JUMP; \
        return AJSON_TOK_NEED_DATA; \
    } \
    else if (input[index] == '\n') { \
        parser->columnno = 0; \
        ++ parser->lineno; \
    } \
    else { \
        ++ parser->columnno; \
    }

#define READ_NEXT() { \
        ++ index; \
        AUTO_STATE(); \
        if (AT_EOF()) { \
            RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED_EOF) \
        } \
        else RQUIRE_DATA(AUTO_STATE_REF()) \
    }

#define READ_NEXT_OR_EOF() { \
        ++ index; \
        AUTO_STATE(); \
        if (AT_EOF()) { \
            /* ignore */ \
        } \
        else RQUIRE_DATA(AUTO_STATE_REF()) \
    }

#define STATE_WITH_DATA(NAME) \
    STATE(NAME) \
    if (AT_EOF()) { \
        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED_EOF) \
    } \
    else RQUIRE_DATA(STATE_REF(NAME))

#define RETURN(TOK) { \
    parser->input_current = index; \
    if (parser->stack_current == 0) { \
        AJSON_SET_ERROR(parser, AJSON_ERROR_EMPTY_SATCK); \
        parser->stack[parser->stack_current] = STATE_REF(ERROR); \
        return AJSON_TOK_ERROR; \
    } \
    -- parser->stack_current; \
    return TOK; \
}

#define RETURN_VALUE(TOK, VALUE) parser->value VALUE; RETURN(TOK)

#define DONE() \
    parser->input_current = index; \
    return AJSON_TOK_END;

#define EMIT(TOK) { \
        parser->input_current = index; \
        parser->stack[parser->stack_current] = AUTO_STATE_REF(); \
        return TOK; \
    } \
    AUTO_STATE();

#define RAISE_ERROR(ERR) { \
    parser->input_current = index; \
    AJSON_SET_ERROR(parser, ERR); \
    parser->stack[parser->stack_current] = STATE_REF(ERROR); \
    return AJSON_TOK_ERROR; \
}

#define RECURSE(ENTER_STATE) { \
    if (ajson_push(parser, AUTO_STATE_REF()) != 0) { \
        parser->input_current = index; \
        parser->stack[parser->stack_current] = STATE_REF(ERROR); \
        return AJSON_TOK_ERROR; \
    } \
    DISPATCH(ENTER_STATE) \
    AUTO_STATE(); \
}

enum ajson_token ajson_next_token(ajson_parser *parser) {
    BEGIN_DISPATCH;

    STATE_WITH_DATA(START)
        while (isspace(CURR_CH())) {
            READ_NEXT();
        }

        RECURSE(VALUE);

        while (!AT_EOF() && isspace(CURR_CH())) {
            READ_NEXT_OR_EOF();
        }

        if (!AT_EOF()) {
            RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED)
        }

        DONE();

    STATE_WITH_DATA(VALUE)
        if (CURR_CH() == 't') {
            // parse "true"
            READ_NEXT();
            if (CURR_CH() == 'r') {
                READ_NEXT();
                if (CURR_CH() == 'u') {
                    READ_NEXT();
                    if (CURR_CH() == 'e') {
                        READ_NEXT_OR_EOF();
                        if (AT_EOF() || ispunct(CURR_CH()) || isspace(CURR_CH())) {
                            RETURN_VALUE(AJSON_TOK_BOOLEAN, .boolean = true);
                        }
                    }
                }
            }
        }
        else if (CURR_CH() == 'f') {
            // parse "false"
            READ_NEXT();
            if (CURR_CH() == 'a') {
                READ_NEXT();
                if (CURR_CH() == 'l') {
                    READ_NEXT();
                    if (CURR_CH() == 's') {
                        READ_NEXT();
                        if (CURR_CH() == 'e') {
                            READ_NEXT_OR_EOF();
                            if (AT_EOF() || ispunct(CURR_CH()) || isspace(CURR_CH())) {
                                RETURN_VALUE(AJSON_TOK_BOOLEAN, .boolean = false);
                            }
                        }
                    }
                }
            }
        }
        else if (CURR_CH() == 'n') {
            // parse "null"
            READ_NEXT();
            if (CURR_CH() == 'u') {
                READ_NEXT();
                if (CURR_CH() == 'l') {
                    READ_NEXT();
                    if (CURR_CH() == 'l') {
                        READ_NEXT_OR_EOF();
                        if (AT_EOF() || ispunct(CURR_CH()) || isspace(CURR_CH())) {
                            RETURN(AJSON_TOK_NULL);
                        }
                    }
                }
            }
        }
        else if (CURR_CH() == '"') {
            STATE(STRING)

            // parse string
            ajson_buffer_clear(parser);
            for (;;) {
                READ_NEXT();
                char ch = CURR_CH();
                if (ch == '\\') {
                    READ_NEXT();
                    ch = CURR_CH();
                    if (ch == '"' || ch == '\\' || ch == '/') {
                        if (ajson_buffer_putc(parser, ch) != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else if (ch == 'b') {
                        if (ajson_buffer_putc(parser, '\b') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else if (ch == 'f') {
                        if (ajson_buffer_putc(parser, '\f') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else if (ch == 'n') {
                        if (ajson_buffer_putc(parser, '\n') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else if (ch == 'r') {
                        if (ajson_buffer_putc(parser, '\r') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else if (ch == 't') {
                        if (ajson_buffer_putc(parser, '\t') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else if (ch == 'u') {
                        // parse UTF-16

#define READ_UNI_HEX() \
    READ_NEXT(); \
    ch = CURR_CH(); \
    if (ch >= '0' && ch <= '9') { \
        ch += -'0'; \
    } \
    else if (ch >= 'a' && ch <= 'f') { \
        ch += -'a' + 10; \
    } \
    else if (ch >= 'A' && ch <= 'F') { \
        ch += -'A' + 10; \
    } \
    else { \
        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED); \
    }

                        READ_UNI_HEX();
                        parser->value.surrogate_pair.unit1 = ch << 12;

                        READ_UNI_HEX();
                        parser->value.surrogate_pair.unit1 |= ch << 8;

                        READ_UNI_HEX();
                        parser->value.surrogate_pair.unit1 |= ch << 4;

                        READ_UNI_HEX();
                        parser->value.surrogate_pair.unit1 |= ch;

                        uint_fast16_t unit1 = parser->value.surrogate_pair.unit1;
                        uint_fast32_t codepoint;
                        if (unit1 >= 0xD800 && unit1 <= 0xDBFF) {
                            // parsing surrogate pairs
                            READ_NEXT();
                            if (CURR_CH() != '\\') {
                                RAISE_ERROR(AJSON_ERROR_PARSER_UNICODE);
                            }

                            READ_NEXT();
                            if (CURR_CH() != 'u') {
                                RAISE_ERROR(AJSON_ERROR_PARSER_UNICODE);
                            }

                            READ_UNI_HEX();
                            parser->value.surrogate_pair.unit2 = ch << 12;

                            READ_UNI_HEX();
                            parser->value.surrogate_pair.unit2 |= ch << 8;

                            READ_UNI_HEX();
                            parser->value.surrogate_pair.unit2 |= ch << 4;

                            READ_UNI_HEX();
                            parser->value.surrogate_pair.unit2 |= ch;

                            uint_fast16_t unit2 = parser->value.surrogate_pair.unit2;
                            if (unit2 < 0xDC00 || unit2 > 0xDFFF) {
                                RAISE_ERROR(AJSON_ERROR_PARSER_UNICODE);
                            }

                            codepoint = (parser->value.surrogate_pair.unit1 << 10) + unit2 - 0x35FDC00;
                        }
                        else if (unit1 > 0x10FFFF) {
                            RAISE_ERROR(AJSON_ERROR_PARSER_UNICODE);
                        }
                        else {
                            codepoint = unit1;
                        }

                        if (ajson_buffer_putcp(parser, codepoint) != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY);
                        }
                    }
                    else {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);
                    }
                }
                else if (ch == '"') {
                    if (ajson_buffer_putc(parser, 0) != 0) {
                        RAISE_ERROR(AJSON_ERROR_MEMORY);
                    }

                    READ_NEXT_OR_EOF();
                    RETURN_VALUE(AJSON_TOK_STRING, .string = parser->buffer);
                }
                else {
                    if (ajson_buffer_putc(parser, ch) != 0) {
                        RAISE_ERROR(AJSON_ERROR_MEMORY);
                    }
                }
            }
        }
        else if (CURR_CH() == '-' || isdigit(CURR_CH())) {
            // parse number
            parser->value.components.positive          = true;
            parser->value.components.exponent_positive = true;
            parser->value.components.isinteger         = true;
            parser->value.components.integer           = 0;
            parser->value.components.decimal           = 0;
            parser->value.components.decimal_places    = 0;
            parser->value.components.exponent          = 0;

            if (CURR_CH() == '-') {
                parser->value.components.positive = false;
                READ_NEXT();
            }

            if (CURR_CH() >= '1' && CURR_CH() <= '9') {
                do {
                    int digit = CURR_CH() - '0';
                    if ((UINT64_MAX - digit) / 10 < parser->value.components.integer) {
                        // number is not a 64bit integer -> parse floating point number
                        parser->value.components.isinteger = false;
                        do {
                            if (parser->value.components.exponent == UINT64_MAX) {
                                // At this point the whole 64bit address space would
                                // be exhausted just for this number, so don't do anything
                                // more intelligent than raise an range error.
                                RAISE_ERROR(AJSON_ERROR_PARSER_RANGE);
                            }
                            parser->value.components.exponent += 1;
                            READ_NEXT_OR_EOF();
                        } while(!AT_EOF() && isdigit(CURR_CH()));
                        break;
                    }
                    parser->value.components.integer *= 10;
                    parser->value.components.integer += digit;
                    READ_NEXT_OR_EOF();
                } while (!AT_EOF() && isdigit(CURR_CH()));
            }
            else if (CURR_CH() == '0') {
                READ_NEXT_OR_EOF();
            }

            if (!AT_EOF() && CURR_CH() == '.') {
                READ_NEXT();

                parser->value.components.isinteger = false;
                if (!isdigit(CURR_CH())) {
                    RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);
                }

                do {
                    int digit = CURR_CH() - '0';
                    if ((UINT64_MAX - digit) / 10 < parser->value.components.decimal ||
                        parser->value.components.decimal_places == INT64_MAX) {
                        // ignore further decimal places
                        do {
                            READ_NEXT_OR_EOF();
                        } while (!AT_EOF() && isdigit(CURR_CH()));
                        break;
                    }
                    parser->value.components.decimal *= 10;
                    parser->value.components.decimal += digit;
                    parser->value.components.decimal_places += 1;
                    READ_NEXT_OR_EOF();
                } while (!AT_EOF() && isdigit(CURR_CH()));
            }

            if (!AT_EOF() && (CURR_CH() == 'e' || CURR_CH() == 'E')) {
                READ_NEXT();

                parser->value.components.isinteger = false;
                if (CURR_CH() == '-') {
                    parser->value.components.exponent_positive = false;
                    READ_NEXT();
                }
                else if (CURR_CH() == '+') {
                    READ_NEXT();
                }

                if (!isdigit(CURR_CH())) {
                    RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);
                }

                do {
                    int digit = CURR_CH() - '0';
                    if ((UINT64_MAX - digit) / 10 < parser->value.components.exponent) {
                        // this will emit infinity or zero (for negative exponents)
                        parser->value.components.exponent = UINT64_MAX;
                        do {
                            READ_NEXT_OR_EOF();
                        } while (!AT_EOF() && isdigit(CURR_CH()));
                    }
                    parser->value.components.exponent *= 10;
                    parser->value.components.exponent += digit;
                    READ_NEXT_OR_EOF();
                } while (!AT_EOF() && isdigit(CURR_CH()));
            }

            if (!AT_EOF() && !ispunct(CURR_CH()) && !isspace(CURR_CH())) {
                RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);
            }

            if ((parser->flags & AJSON_FLAG_INTEGER) && parser->value.components.isinteger) {
                parser->value.integer = parser->value.components.positive ? parser->value.components.integer : -parser->value.components.integer;
                RETURN(AJSON_TOK_INTEGER);
            }
            else if ((parser->flags & AJSON_FLAG_NUMBER_COMPONENTS) == 0) {
                double number = (double)parser->value.components.integer;

                if (parser->value.components.decimal > 0) {
                    number += parser->value.components.decimal * pow(10.0, -(double)parser->value.components.decimal_places);
                }

                if (parser->value.components.exponent > 0) {
                    if (parser->value.components.exponent_positive) {
                        number *= pow(10.0, (double)parser->value.components.exponent);
                    }
                    else {
                        number *= pow(10.0, -(double)parser->value.components.exponent);
                    }
                }

                if (!parser->value.components.positive) {
                    number = -number;
                }

                parser->value.number = number;
            }

            RETURN(AJSON_TOK_NUMBER);
        }
        else if (CURR_CH() == '[') {
            // parse array
            EMIT(AJSON_TOK_BEGIN_ARRAY);

            do {
                READ_NEXT();
            } while (isspace(CURR_CH()));

            if (CURR_CH() != ']') {
                for (;;) {
                    RECURSE(VALUE);

                    while (!AT_EOF() && isspace(CURR_CH())) {
                        READ_NEXT();
                    }

                    if (AT_EOF()) {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED_EOF);
                    }

                    if (CURR_CH() != ',')
                        break;

                    do {
                        READ_NEXT();
                    } while (isspace(CURR_CH()));
                }
            }

            if (AT_EOF() || CURR_CH() != ']') {
                RAISE_ERROR(AJSON_ERROR_PARSER_EXPECTED_ARRAY_END);
            }

            READ_NEXT_OR_EOF();
            RETURN(AJSON_TOK_END_ARRAY);
        }
        else if (CURR_CH() == '{') {
            // parse object
            EMIT(AJSON_TOK_BEGIN_OBJECT);

            do {
                READ_NEXT();
            } while (isspace(CURR_CH()));

            if (CURR_CH() != '}') {
                for (;;) {
                    if (CURR_CH() != '"') {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);
                    }

                    RECURSE(STRING);

                    while (!AT_EOF() && isspace(CURR_CH())) {
                        READ_NEXT();
                    }

                    if (AT_EOF()) {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED_EOF);
                    }

                    if (CURR_CH() != ':') {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);
                    }

                    do {
                        READ_NEXT();
                    } while (isspace(CURR_CH()));

                    RECURSE(VALUE);

                    while (!AT_EOF() && isspace(CURR_CH())) {
                        READ_NEXT();
                    }

                    if (AT_EOF()) {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED_EOF);
                    }

                    if (CURR_CH() != ',')
                        break;

                    do {
                        READ_NEXT();
                    } while (isspace(CURR_CH()));
                }
            }

            if (AT_EOF() || CURR_CH() != '}') {
                RAISE_ERROR(AJSON_ERROR_PARSER_EXPECTED_OBJECT_END);
            }

            READ_NEXT_OR_EOF();
            RETURN(AJSON_TOK_END_OBJECT);
        }

        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED);

    STATE(ERROR)
        RAISE_ERROR(AJSON_ERROR_PARSER);

    END_DISPATCH;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
