#include "ajson.h"

#include <errno.h>
#include <string.h>
#include <ctype.h>

#define AJSON_STACK_SIZE 64

enum ajson_state {
    AJSON_STATE_START,

    AJSON_STATE_SPACE,
    AJSON_STATE_TRAILING_SPACE,

    AJSON_STATE_VALUE,

    AJSON_STATE_JNULL,
    AJSON_STATE_TRUE,
    AJSON_STATE_FALSE,
    AJSON_STATE_NUMBER,
    AJSON_STATE_INTEGER,
    AJSON_STATE_STRING,

    AJSON_STATE_WORD_n,
    AJSON_STATE_WORD_nu,
    AJSON_STATE_WORD_nul,
    AJSON_STATE_WORD_null,

    AJSON_STATE_WORD_t,
    AJSON_STATE_WORD_tr,
    AJSON_STATE_WORD_tru,
    AJSON_STATE_WORD_true,

    AJSON_STATE_WORD_f,
    AJSON_STATE_WORD_fa,
    AJSON_STATE_WORD_fal,
    AJSON_STATE_WORD_fals,
    AJSON_STATE_WORD_false,

    AJSON_STATE_NUMBER_NEGATIVE,
    AJSON_STATE_NUMBER_LEADING_ZERO,
    AJSON_STATE_NUMBER_LEADING_NONZERO,
    AJSON_STATE_NUMBER_DIGIT,
    AJSON_STATE_NUMBER_DOT,
    AJSON_STATE_NUMBER_DECIMAL,
    AJSON_STATE_NUMBER_EXP,
    AJSON_STATE_NUMBER_EXP_POS,
    AJSON_STATE_NUMBER_EXP_NEG,
    AJSON_STATE_NUMBER_EXP_DIGIT,

    AJSON_STATE_STRING_BEGIN,
    AJSON_STATE_STRING_CHAR,
    AJSON_STATE_STRING_ESC,
    AJSON_STATE_STRING_ESC_QUOTE,
    AJSON_STATE_STRING_ESC_BACKSLASH,
    AJSON_STATE_STRING_ESC_SLASH,
    AJSON_STATE_STRING_ESC_BACKSPACE,
    AJSON_STATE_STRING_ESC_FORMFEED,
    AJSON_STATE_STRING_ESC_NEWLINE,
    AJSON_STATE_STRING_ESC_CARRIGERET,
    AJSON_STATE_STRING_ESC_TAB,

    AJSON_STATE_UNICODE_BEGIN,
    AJSON_STATE_UNICODE_HEX1,
    AJSON_STATE_UNICODE_HEX2,
    AJSON_STATE_UNICODE_HEX3,
    AJSON_STATE_UNICODE_HEX4,

    AJSON_STATE_ARRAY_BEGIN,
    AJSON_STATE_ARRAY_AFTER_EMIT,
    AJSON_STATE_ARRAY_SPACE_BEFORE,
    AJSON_STATE_ARRAY_SPACE_BEFORE_COMMA,
    AJSON_STATE_ARRAY_COMMA,
    AJSON_STATE_ARRAY_SPACE_AFTER_COMMA,
    AJSON_STATE_ARRAY_VALUE,

    AJSON_STATE_OBJECT_BEGIN,
    AJSON_STATE_OBJECT_COLON,
    AJSON_STATE_OBJECT_COMMA,

    AJSON_STATE_ERROR,

    AJSON_STATE_COUNT
};

const char* ajson_error_str(enum ajson_error error) {
    switch (error) {
    case AJSON_ERROR_EMPTY_SATCK:
        return "empty stack";

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

    parser->stack = calloc(AJSON_STACK_SIZE, sizeof(enum ajson_state));
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

static inline int ajson_push(ajson_parser *parser, uintptr_t state) {
    if (parser->stack_current + 1 == parser->buffer_size) {
        size_t newsize = parser->stack_size + AJSON_STACK_SIZE;
        uintptr_t *stack = SIZE_MAX/sizeof(uintptr_t) < newsize ? NULL : realloc(parser->stack, sizeof(uintptr_t) * newsize);
        if (stack == NULL) {
            parser->value.error = AJSON_ERROR_MEMORY;
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
        size_t newsize = needed / BUFSIZ;
        newsize *= BUFSIZ;
        if (newsize != needed) {
            newsize += BUFSIZ;
        }
        char *buffer = realloc(parser->buffer, newsize);
        if (buffer == NULL) {
            parser->value.error = AJSON_ERROR_MEMORY;
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
            parser->value.error = AJSON_ERROR_MEMORY;
            return -1;
        }
        parser->buffer = buffer;
        parser->buffer_size = newsize;
    }
    parser->buffer[parser->buffer_used ++] = ch;
    return 0;
}

static inline int ajson_buffer_putcp(ajson_parser *parser, uint32_t codepoint) {
    // TODO
}

#ifdef __GNUC__
#define AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#endif

enum ajson_token ajson_next_token(ajson_parser *parser) {
    const char*  input = parser->input;
    size_t       index = parser->input_current;
    const size_t size  = parser->input_size;

#ifdef AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#pragma GCC diagnostic ignored "-pedantic"
#pragma GCC diagnostic ignored "-Wunused-label"
    // http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
    // use offsets instead of absolute addresses to reduce the number of
    // dynamic relocations for code in shared libraries

#   define STATE_REF(NAME) (&&do_ ## NAME - &&do_START)
#   define STATE(NAME) do_ ## NAME :
#   define BEGIN_DISPATCH goto *(&&do_START + parser->stack[parser->stack_current]);
#   define END_DISPATCH
#   define DISPATCH(NAME) goto do_ ## NAME;

#else

#   define STATE_REF(NAME) AJSON_STATE_ ## NAME
#   define STATE(NAME) case STATE_REF(NAME):
#   define BEGIN_DISPATCH uintptr_t state = parser->stack[parser->stack_current]; dispatch_loop: switch (state) {
#   define END_DISPATCH }
#   define DISPATCH(NAME) state = AJSON_STATE_ ## NAME; goto dispatch_loop;

#endif

#define RQUIRE_DATA(NAME) \
    if (index == size) { \
        parser->input_current = index; \
        parser->stack[parser->stack_current] = STATE_REF(NAME); \
        return AJSON_TOK_EOF; \
    } \
    else if (input[index] == '\n') { \
        parser->columnno = 0; \
        ++ parser->lineno; \
    } \
    else { \
        ++ parser->columnno; \
    }

#define READ_NEXT(NAME) \
    ++ index; \
    STATE(NAME) \
    if (size == 0) { \
        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED_EOF) \
    } \
    else RQUIRE_DATA(NAME)

#define READ_NEXT_OR_EOF(NAME) \
    ++ index; \
    STATE(NAME) \
    if (size == 0) { \
        /* EOF */ \
    } \
    else RQUIRE_DATA(NAME)

#define STATE_WITH_DATA(NAME) \
    STATE(NAME) \
    RQUIRE_DATA(NAME)

#define RETURN(TOK) { \
    parser->input_current = index; \
    if (parser->stack_current == 0) { \
        parser->value.error = AJSON_ERROR_EMPTY_SATCK; \
        parser->stack[parser->stack_current] = STATE_REF(ERROR); \
        return AJSON_TOK_ERROR; \
    } \
    -- parser->stack_current; \
    return TOK; \
}

#define RETURN_VALUE(TOK, VALUE) parser->value VALUE; RETURN(TOK)

#define EMIT(TOK, NAME) { \
        parser->input_current = index; \
        parser->stack[parser->stack_current] = STATE_REF(NAME); \
        return TOK; \
    } \
    STATE(NAME)

#define RAISE_ERROR(ERR) { \
    parser->input_current = index; \
    parser->value.error = ERR; \
    parser->stack[parser->stack_current] = STATE_REF(ERROR); \
    return AJSON_TOK_ERROR; \
}

#define ENTER(TOK, NAME) { \
    parser->input_current = index; \
    if (ajson_push(parser, STATE_REF(NAME)) != 0) { \
        parser->stack[parser->stack_current] = STATE_REF(ERROR); \
        return AJSON_TOK_ERROR; \
    } \
    return TOK; \
    STATE(NAME) \
    (void)0; \
}

#define RECURSE(ENTER_STATE, RETURN_STATE) { \
    if (ajson_push(parser, STATE_REF(RETURN_STATE)) != 0) { \
        parser->input_current = index; \
        parser->stack[parser->stack_current] = STATE_REF(ERROR); \
        return AJSON_TOK_ERROR; \
    } \
    DISPATCH(ENTER_STATE) \
    STATE(RETURN_STATE) \
    (void)0; \
}

    BEGIN_DISPATCH

    STATE_WITH_DATA(START)
        if (ajson_push(parser, STATE_REF(VALUE)) != 0) {
            parser->stack[parser->stack_current] = STATE_REF(ERROR);
            return AJSON_TOK_ERROR;
        }

        while (isspace(input[index])) {
            READ_NEXT(SPACE)
        }

    STATE_WITH_DATA(VALUE)
        if (input[index] == 't') {
            // parse "true"
            READ_NEXT(WORD_t)
            if (input[index] == 'r') {
                READ_NEXT(WORD_tr)
                if (input[index] == 'u') {
                    READ_NEXT(WORD_tru)
                    if (input[index] == 'e') {
                        READ_NEXT_OR_EOF(WORD_true)
                        if (size == 0 || ispunct(input[index]) || isspace(input[index])) {
                            RETURN_VALUE(AJSON_TOK_BOOLEAN, .boolean = true)
                        }
                    }
                }
            }
        }
        else if (input[index] == 'f') {
            // parse "false"
            READ_NEXT(WORD_f)
            if (input[index] == 'a') {
                READ_NEXT(WORD_fa)
                if (input[index] == 'l') {
                    READ_NEXT(WORD_fal)
                    if (input[index] == 's') {
                        READ_NEXT(WORD_fals)
                        if (input[index] == 'e') {
                            READ_NEXT_OR_EOF(WORD_false)
                            if (size == 0 || ispunct(input[index]) || isspace(input[index])) {
                                RETURN_VALUE(AJSON_TOK_BOOLEAN, .boolean = false)
                            }
                        }
                    }
                }
            }
        }
        else if (input[index] == 'n') {
            // parse "null"
            READ_NEXT(WORD_n)
            if (input[index] == 'u') {
                READ_NEXT(WORD_nu)
                if (input[index] == 'l') {
                    READ_NEXT(WORD_nul)
                    if (input[index] == 'l') {
                        READ_NEXT_OR_EOF(WORD_null)
                        if (size == 0 || ispunct(input[index]) || isspace(input[index])) {
                            RETURN(AJSON_TOK_NULL)
                        }
                    }
                }
            }
        }
        else if (input[index] == '"') {
            // parse string
            for (;;) {
                READ_NEXT(STRING_CHAR)
                char ch = input[index];
                if (ch == '\\') {
                    READ_NEXT(STRING_ESC)
                    if (ch == '"' || ch == '\\' || ch == '/') {
                        if (ajson_buffer_putc(parser, ch) != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }
                    }
                    else if (ch == 'b') {
                        if (ajson_buffer_putc(parser, '\b') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }
                    }
                    else if (ch == 'f') {
                        if (ajson_buffer_putc(parser, '\f') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }
                    }
                    else if (ch == 'n') {
                        if (ajson_buffer_putc(parser, '\n') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }
                    }
                    else if (ch == 'r') {
                        if (ajson_buffer_putc(parser, '\r') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }
                    }
                    else if (ch == 't') {
                        if (ajson_buffer_putc(parser, '\t') != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }
                    }
                    else if (ch == 'u') {
                        // parse UTF-16 unicode
                        uint32_t codepoint = 0;

                        READ_NEXT(UNICODE_HEX1)
                        ch = input[index];
                        if (ch >= '0' && ch <= '9') {
                            codepoint = (ch - '0') << 12;
                        }
                        else if (ch >= 'a' && ch <= 'f') {
                            codepoint = (ch - 'a' + 10) << 12;
                        }
                        else if (ch >= 'A' && ch <= 'F') {
                            codepoint = (ch - 'A' + 10) << 12;
                        }
                        else {
                            RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED)
                        }

                        // TODO: parse UTF-16 surrogate

                        if (codepoint < 0x10000) {
                            // done
                        }
                        else if (codepoint < 0x10FFFF) {
                            // needs another surrogate
                        }
                        else {
                            RAISE_ERROR(AJSON_ERROR_PARSER_UNICODE)
                        }

                        if (ajson_buffer_putcp(parser, codepoint) != 0) {
                            RAISE_ERROR(AJSON_ERROR_MEMORY)
                        }

                        // TOOD: parse UTF-16, write UTF-8 to buffer
                    }
                    else {
                        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED)
                    }
                    // TODO
                }
                else if (ch == '"') {
                    ++ index;
                    if (ajson_buffer_putc(parser, 0) != 0) {
                        RAISE_ERROR(AJSON_ERROR_MEMORY)
                    }
                    RETURN_VALUE(AJSON_TOK_STRING, .string = parser->buffer)
                }
                else {
                    if (ajson_buffer_putc(parser, ch) != 0) {
                        RAISE_ERROR(AJSON_ERROR_MEMORY)
                    }
                }
            }
            // TODO
        }
        else if (input[index] == '-') {
            // parse number (sign)
            // TODO
        }
        else if (isdigit(input[index])) {
            // parse number
            // TODO
        }
        else if (input[index] == '[') {
            // parse array
            READ_NEXT(ARRAY_BEGIN)
            EMIT(AJSON_TOK_BEGIN_ARRAY, ARRAY_AFTER_EMIT)

            while (isspace(input[index])) {
                READ_NEXT(ARRAY_SPACE_BEFORE)
            }

            if (input[index] != ']') {
                for (;;) {
                    RECURSE(VALUE, ARRAY_VALUE)

                    while (isspace(input[index])) {
                        READ_NEXT(ARRAY_SPACE_BEFORE_COMMA)
                    }

                    if (input[index] != ',')
                        break;

                    READ_NEXT(ARRAY_COMMA)
                    while (isspace(input[index])) {
                        READ_NEXT(ARRAY_SPACE_AFTER_COMMA)
                    }
                }
            }

            if (input[index] != ']') {
                RAISE_ERROR(AJSON_ERROR_PARSER_EXPECTED_ARRAY_END)
            }

            ++ index;
            RETURN(AJSON_TOK_END_ARRAY)
        }
        else if (input[index] == '{') {
            // parse object
            // TODO
        }

        RAISE_ERROR(AJSON_ERROR_PARSER_UNEXPECTED)

    STATE(ERROR)
        printf("error: in error state\n");
        RAISE_ERROR(AJSON_ERROR_PARSER)

    END_DISPATCH
    printf("error: shouldn't be here\n");
    RAISE_ERROR(AJSON_ERROR_PARSER)
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
