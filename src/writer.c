#include "ajson.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>

#ifndef HAVE_SNPRINTF
#	include "snprintf.h"
#endif

#define NOT_FIRST 32

static int ajson_writer_push(ajson_writer *writer, char type);
static int ajson_writer_pop( ajson_writer *writer);

static ssize_t _ajson_write_prelude(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_dummy  (ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_null   (ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_boolean(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_number (ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_integer(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_string (ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);

static ssize_t _ajson_write_begin_array(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_end_array  (ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);

static ssize_t _ajson_write_begin_object(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_end_object  (ajson_writer *writer, unsigned char *buffer, size_t size, size_t index);


int         ajson_writer_get_flags (ajson_writer *writer) { return writer->flags;  }
const char *ajson_writer_get_indent(ajson_writer *writer) { return writer->indent; }

int ajson_writer_init(ajson_writer *writer, int flags, const char *indent) {
    if (flags & ~AJSON_WRITER_FLAGS_ALL) {
        errno = EINVAL;
        return -1;
    }

    if (indent) {
        for (const char *ptr = indent; *ptr; ++ ptr) {
            if (!isspace(*ptr)) {
                errno = EINVAL;
                return -1;
            }
        }
    }

    memset(writer, 0, sizeof(ajson_writer));
    writer->flags      = flags;
    writer->indent     = indent;
    writer->write_func = &_ajson_write_dummy;
    writer->stack      = calloc(AJSON_STACK_SIZE, 1);

    if (!writer->stack) {
        return -1;
    }

    writer->stack_size = AJSON_STACK_SIZE;
    return 0;
}

void ajson_writer_reset(ajson_writer *writer) {
    writer->nesting_written = 0;
    writer->write_func      = &_ajson_write_dummy;
    writer->next_write_func = NULL;
    writer->state           = 0;
    writer->stack_current   = 0;
    if (writer->stack && writer->stack_size > 0) {
        writer->stack[0] = 0;
    }
}

void ajson_writer_destroy(ajson_writer *writer) {
    free(writer->stack);
    writer->stack         = NULL;
    writer->stack_size    = 0;
    writer->stack_current = 0;
}

ajson_writer *ajson_writer_alloc(int flags, const char *indent) {
    ajson_writer *writer = malloc(sizeof(ajson_writer));

    if (writer) {
        if (ajson_writer_init(writer, flags, indent) != 0) {
            free(writer);
            return NULL;
        }
    }

    return writer;
}

void ajson_writer_free(ajson_writer *writer) {
    ajson_writer_destroy(writer);
    free(writer);
}

ssize_t ajson_write_null(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_null;
    writer->state           = 0;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_boolean(ajson_writer *writer, void *buffer, size_t size, bool value) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_boolean;
    writer->state           = 0;
    writer->value.boolean   = value;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_number(ajson_writer *writer, void *buffer, size_t size, double value) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_number;
    writer->state           = 0;
    writer->value.number.value = value;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_integer(ajson_writer *writer, void *buffer, size_t size, int64_t value) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_integer;
    writer->state           = 0;
    writer->value.integer.value.sval = value;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_string(ajson_writer *writer, void *buffer, size_t size, const char* value, size_t length, enum ajson_encoding encoding) {
    if (size == 0 || size > SSIZE_MAX || !value) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_string;
    writer->state           = 0;
    writer->value.string.value    = value;
    writer->value.string.end      = value + length;
    writer->value.string.encoding = encoding;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_string_latin1(ajson_writer *writer, void *buffer, size_t size, const char* value) {
    return ajson_write_string(writer, buffer, size, value, strlen(value), AJSON_ENC_LATIN1);
}

ssize_t ajson_write_string_utf8(ajson_writer *writer, void *buffer, size_t size, const char* value) {
    return ajson_write_string(writer, buffer, size, value, strlen(value), AJSON_ENC_UTF8);
}

ssize_t ajson_write_begin_array(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_begin_array;
    writer->state           = 0;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_end_array(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_end_array;
    writer->next_write_func = NULL;
    writer->state           = 0;
    return _ajson_write_end_array(writer, buffer, size, 0);
}

ssize_t ajson_write_begin_object(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX || (writer->stack[writer->stack_current] | NOT_FIRST) == 'o') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_begin_object;
    writer->state           = 0;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_end_object(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX || writer->stack[writer->stack_current] == 'k') {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_end_object;
    writer->next_write_func = NULL;
    writer->state           = 0;
    return _ajson_write_end_object(writer, buffer, size, 0);
}

ssize_t ajson_write_continue(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    return writer->write_func(writer, buffer, size, 0);
}

int ajson_writer_push(ajson_writer *writer, char type) {
    if (writer->stack_size == writer->stack_current + 1) {
        size_t newsize = writer->stack_size + AJSON_STACK_SIZE;
        char *stack = realloc(writer->stack, newsize);

        if (!stack) {
            return -1;
        }

        writer->stack      = stack;
        writer->stack_size = newsize;
    }

    writer->stack[++ writer->stack_current] = type;
    return 0;
}

int ajson_writer_pop(ajson_writer *writer) {
    if (writer->stack_current == 0) {
        errno = EINVAL;
        return -1;
    }
    return writer->stack[writer->stack_current --];
}

#ifdef AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#   pragma GCC diagnostic ignored "-pedantic"
    // http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
    // use offsets instead of absolute addresses to reduce the number of
    // dynamic relocations for code in shared libraries

#   define AUTO_STATE_NAME2(LINE) do_ ## LINE
#   define AUTO_STATE_NAME(LINE) AUTO_STATE_NAME2(LINE)
#   define STATE_REF(NAME) (&&do_named_ ## NAME - &&do_START)
#   define STATE(NAME) do_named_ ## NAME :
#   define AUTO_STATE_REF() (&&AUTO_STATE_NAME(__LINE__) - &&do_START)
#   define AUTO_STATE() AUTO_STATE_NAME(__LINE__):
#   define BEGIN_DISPATCH goto *(&&do_START + writer->state); do_START: {

#else

enum ajson_named_states {
    AJSON_STATE_START,

    AJSON_STATECOUNT
};

#   define STATE_REF(NAME) AJSON_STATE_ ## NAME
#   define STATE(NAME) case STATE_REF(NAME):
#   define AUTO_STATE_REF() (AJSON_STATECOUNT + __LINE__)
#   define AUTO_STATE() case AJSON_STATECOUNT + __LINE__:
#   define BEGIN_DISPATCH switch (writer->state) { case AJSON_STATE_START:

#endif

#define END_DISPATCH  AUTO_STATE(); writer->state = AUTO_STATE_REF(); } return index;
#define RAISE_ERROR() AUTO_STATE(); writer->state = AUTO_STATE_REF(); return -1;
#define CONTINUE(NAME) writer->state = STATE_REF(NAME); return index;
#define TO_HEX(CH) ((CH) > 9 ? 'a' + (CH) - 10 : '0' + (CH))

#define WRITE_CHAR(CH) \
    writer->buffer.character = (CH); \
    AUTO_STATE(); \
    if (index == size) { \
        writer->state = AUTO_STATE_REF(); \
        return size; \
    } \
    buffer[index ++] = writer->buffer.character;

#define WRITE_STR(STR) { \
    writer->buffer.string = (STR); \
    AUTO_STATE(); \
    const char *str = writer->buffer.string; \
    size_t nstr  = strlen(str); \
    size_t nfree = size - index; \
    if (nstr > nfree) { \
        memcpy(buffer + index, str, nfree); \
        writer->state = AUTO_STATE_REF(); \
        writer->buffer.string = str + nfree; \
        return size; \
    } \
    else { \
        memcpy(buffer + index, str, nstr); \
        index += nstr; \
    } \
}

ssize_t _ajson_write_prelude(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    int curr = writer->stack[writer->stack_current];
    if ((curr | NOT_FIRST) == 'a' || (curr | NOT_FIRST) == 'o') {
        if (curr & NOT_FIRST) {
            WRITE_CHAR(',');
        }
        curr = writer->stack[writer->stack_current];
        if ((curr | NOT_FIRST) == 'o') {
            writer->stack[writer->stack_current] = 'k';
        }
        else if (curr == 'A') {
            writer->stack[writer->stack_current] = 'a';
        }
        if (writer->indent) {
            WRITE_CHAR('\n');
            if (*writer->indent) {
                for (writer->nesting_written = 0; writer->nesting_written < writer->stack_current; ++ writer->nesting_written) {
                    WRITE_STR(writer->indent);
                }
            }
        }
    }
    else if (curr == 'k') {
        writer->stack[writer->stack_current] = 'o';
        if (writer->indent) {
            WRITE_STR(": ");
        }
        else {
            WRITE_CHAR(':');
        }
    }

    writer->write_func = writer->next_write_func;
    writer->state      = 0;

    return writer->write_func(writer, buffer, size, index);

    END_DISPATCH;
}

ssize_t _ajson_write_dummy(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    (void)ajson_writer_push;
    (void)ajson_writer_pop;
    (void)writer;
    (void)buffer;
    (void)size;
    (void)index;
    return 0;
}

ssize_t _ajson_write_null(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    WRITE_STR("null");

    END_DISPATCH;
}

ssize_t _ajson_write_boolean(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    WRITE_STR(writer->value.boolean ? "true" : "false");

    END_DISPATCH;
}

ssize_t _ajson_write_number(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (isfinite(writer->value.number.value)) {
        size_t nfree = size - index;

        if (nfree >= sizeof(writer->value.number.buffer)) {
            // this code path saves one memcpy
            int count = snprintf((char*)buffer + index, nfree, "%.16g", writer->value.number.value);
            if (count < 0) {
                RAISE_ERROR();
            }
            else if ((size_t)count > nfree) {
                errno = ENOBUFS;
                RAISE_ERROR();
            }

            index += (size_t)count;
        }
        else {
            writer->value.number.written = 0;

            int count = snprintf(writer->value.number.buffer, sizeof(writer->value.number.buffer), "%.16g", writer->value.number.value);
            if (count < 0) {
                RAISE_ERROR();
            }
            else if ((size_t)count > sizeof(writer->value.number.buffer)) {
                errno = ENOBUFS;
                RAISE_ERROR();
            }

            WRITE_STR(writer->value.number.buffer);
        }
    }
    else {
        WRITE_STR("null");
    }

    END_DISPATCH;
}

ssize_t _ajson_write_integer(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (writer->value.integer.value.sval == 0) {
        WRITE_CHAR('0');
    }
    else {
        uint64_t integer;
        if (writer->value.integer.value.sval < 0) {
            WRITE_CHAR('-');
            integer = -writer->value.integer.value.sval;
        }
        else {
            integer = writer->value.integer.value.sval;
        }

        uint64_t div = 10000000000000000000LU;
        while ((integer / div) == 0) {
            div /= 10;
        }

        writer->value.integer.value.uval = integer;
        writer->value.integer.div        = div;

        while (writer->value.integer.div > 0) {
            WRITE_CHAR('0' + (writer->value.integer.value.uval / writer->value.integer.div) % 10);
            writer->value.integer.div /= 10;
        }
    }

    END_DISPATCH;
}

ssize_t _ajson_write_string(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    WRITE_CHAR('"');

    while (writer->value.string.value != writer->value.string.end) {
        unsigned char ch = *writer->value.string.value;

        if (ch == '"') {
            WRITE_STR("\\\"");
            writer->value.string.value += 1;
        }
        else if (ch == '\\') {
            WRITE_STR("\\\\");
            writer->value.string.value += 1;
        }
        else if (ch == '\b') {
            WRITE_STR("\\b");
            writer->value.string.value += 1;
        }
        else if (ch == '\f') {
            WRITE_STR("\\f");
            writer->value.string.value += 1;
        }
        else if (ch == '\n') {
            WRITE_STR("\\n");
            writer->value.string.value += 1;
        }
        else if (ch == '\r') {
            WRITE_STR("\\r");
            writer->value.string.value += 1;
        }
        else if (ch == '\t') {
            WRITE_STR("\\t");
            writer->value.string.value += 1;
        }
        else if (ch <= 0x1F || ch == 0x7F || (writer->value.string.encoding == AJSON_ENC_LATIN1 && ch >= 0x80 && ch <= 0x9F)) {
            WRITE_STR("\\u00");

            ch = *writer->value.string.value >> 4;
            WRITE_CHAR(TO_HEX(ch));

            ch = *writer->value.string.value & 0x0F;
            WRITE_CHAR(TO_HEX(ch));

            writer->value.string.value += 1;
        }
        else if (ch < 0x7F || writer->value.string.encoding == AJSON_ENC_LATIN1) {
            WRITE_CHAR(ch);
            writer->value.string.value += 1;
        }
        else {
            uint32_t codepoint = 0;
            int count = ajson_decode_utf8((const unsigned char*)writer->value.string.value, writer->value.string.end - writer->value.string.value, &codepoint);
            if (count <= 0) {
                RAISE_ERROR();
            }

            if (writer->flags & AJSON_WRITER_FLAG_ASCII || (codepoint >= 0x80 && codepoint <= 0x9F)) {
                // encode as \u####(\u####), i.e. ASCII encoded UTF-16
                writer->value.string.value += count;

                if (codepoint < 0x10000) {
                    writer->value.string.buffer.utf16.unit1 = codepoint;
                    writer->value.string.buffer.utf16.unit2 = 0;
                }
                else {
                    writer->value.string.buffer.utf16.unit1 = (codepoint >> 10)   + 0xD7C0;
                    writer->value.string.buffer.utf16.unit2 = (codepoint & 0x3FF) + 0xDC00;
                }

                WRITE_STR("\\u");

                ch = (writer->value.string.buffer.utf16.unit1 >> 12) & 0x0F;
                WRITE_CHAR(TO_HEX(ch));

                ch = (writer->value.string.buffer.utf16.unit1 >> 8) & 0x0F;
                WRITE_CHAR(TO_HEX(ch));

                ch = (writer->value.string.buffer.utf16.unit1 >> 4) & 0x0F;
                WRITE_CHAR(TO_HEX(ch));

                ch = writer->value.string.buffer.utf16.unit1 & 0x0F;
                WRITE_CHAR(TO_HEX(ch));

                if (writer->value.string.buffer.utf16.unit2) {
                    WRITE_STR("\\u");

                    ch = (writer->value.string.buffer.utf16.unit2 >> 12) & 0x0F;
                    WRITE_CHAR(TO_HEX(ch));

                    ch = (writer->value.string.buffer.utf16.unit2 >> 8) & 0x0F;
                    WRITE_CHAR(TO_HEX(ch));

                    ch = (writer->value.string.buffer.utf16.unit2 >> 4) & 0x0F;
                    WRITE_CHAR(TO_HEX(ch));

                    ch = writer->value.string.buffer.utf16.unit2 & 0x0F;
                    WRITE_CHAR(TO_HEX(ch));
                }
            }
            else {
                // copy input because it is already UTF-8
                for (writer->value.string.buffer.count = count;
                     writer->value.string.buffer.count > 0;
                     -- writer->value.string.buffer.count) {
                    WRITE_CHAR(*writer->value.string.value);
                    writer->value.string.value += 1;
                }
            }
        }
    }

    WRITE_CHAR('"');

    END_DISPATCH;
}

ssize_t _ajson_write_begin_array(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (ajson_writer_push(writer, 'A') != 0) {
        RAISE_ERROR();
    }

    WRITE_CHAR('[');

    END_DISPATCH;
}

ssize_t _ajson_write_end_array(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    int which = ajson_writer_pop(writer);
    if ((which | NOT_FIRST) != 'a') {
        errno = EINVAL;
        RAISE_ERROR();
    }

    if (which == 'a' && writer->indent) {
        WRITE_CHAR('\n');
        if (*writer->indent) {
            for (writer->nesting_written = 0; writer->nesting_written < writer->stack_current; ++ writer->nesting_written) {
                WRITE_STR(writer->indent);
            }
        }
    }

    WRITE_CHAR(']');

    END_DISPATCH;
}

ssize_t _ajson_write_begin_object(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (ajson_writer_push(writer, 'O') != 0) {
        RAISE_ERROR();
    }

    WRITE_CHAR('{');

    END_DISPATCH;
}

ssize_t _ajson_write_end_object(ajson_writer *writer, unsigned char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    int which = ajson_writer_pop(writer);
    if ((which | NOT_FIRST) != 'o') {
        errno = EINVAL;
        RAISE_ERROR();
    }

    if (which == 'o' && writer->indent) {
        WRITE_CHAR('\n');
        if (*writer->indent) {
            for (writer->nesting_written = 0; writer->nesting_written < writer->stack_current; ++ writer->nesting_written) {
                WRITE_STR(writer->indent);
            }
        }
    }

    WRITE_CHAR('}');

    END_DISPATCH;
}
#ifdef AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#   pragma GCC diagnostic pop
#endif
