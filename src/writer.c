#include "ajson.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#define NOT_FIRST 32

static int ajson_writer_push(ajson_writer *writer, char type);
static int ajson_writer_pop( ajson_writer *writer);

static ssize_t _ajson_write_prelude(ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_dummy  (ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_null   (ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_boolean(ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_number (ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_integer(ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_string (ajson_writer *writer, char *buffer, size_t size, size_t index);

static ssize_t _ajson_write_begin_array(ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_end_array  (ajson_writer *writer, char *buffer, size_t size, size_t index);

static ssize_t _ajson_write_begin_object(ajson_writer *writer, char *buffer, size_t size, size_t index);
static ssize_t _ajson_write_end_object  (ajson_writer *writer, char *buffer, size_t size, size_t index);

int ajson_writer_init(ajson_writer *writer, const char *indent) {
    memset(writer, 0, sizeof(ajson_writer));
    writer->indent     = indent;
    writer->write_func = &_ajson_write_dummy;
    writer->stack      = calloc(AJSON_STACK_SIZE, 1);

    if (!writer->stack) {
        return -1;
    }

    writer->stack_size = AJSON_STACK_SIZE;
    return 0;
}

void ajson_writer_destroy(ajson_writer *writer) {
    free(writer->stack);
    writer->stack         = NULL;
    writer->stack_size    = 0;
    writer->stack_current = 0;
}

ajson_writer *ajson_writer_alloc(const char *indent) {
    ajson_writer *writer = malloc(sizeof(ajson_writer));

    if (writer) {
        if (ajson_writer_init(writer, indent) != 0) {
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
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_null;
    writer->state           = 0;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_boolean(ajson_writer *writer, void *buffer, size_t size, bool value) {
    if (size == 0 || size > SSIZE_MAX) {
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
    if (size == 0 || size > SSIZE_MAX) {
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
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_integer;
    writer->state           = 0;
    writer->value.integer.value.sval = value;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_string(ajson_writer *writer, void *buffer, size_t size, const char* value) {
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_string;
    writer->state           = 0;
    writer->value.string    = value;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_begin_array(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_begin_array;
    writer->state           = 0;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_end_array(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_end_array;
    writer->next_write_func = NULL;
    writer->state           = 0;
    return _ajson_write_end_array(writer, buffer, size, 0);
}

ssize_t ajson_write_begin_object(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    writer->write_func      = &_ajson_write_prelude;
    writer->next_write_func = &_ajson_write_begin_object;
    writer->state           = 0;
    return _ajson_write_prelude(writer, buffer, size, 0);
}

ssize_t ajson_write_end_object(ajson_writer *writer, void *buffer, size_t size) {
    if (size == 0 || size > SSIZE_MAX) {
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
    if (writer->stack_size == writer->stack_current) {
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

#ifdef __GNUC__
#   define AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#endif

#ifdef AJSON_USE_GNUC_ADDRESS_FROM_LABEL
#pragma GCC diagnostic ignored "-pedantic"
//#pragma GCC diagnostic ignored "-Wunused-label"
    // http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
    // use offsets instead of absolute addresses to reduce the number of
    // dynamic relocations for code in shared libraries

#   define AUTO_STATE_NAME2(LINE) do_ ## LINE
#   define AUTO_STATE_NAME(LINE) AUTO_STATE_NAME2(LINE)
#   define STATE_REF(NAME) (&&do_named_ ## NAME - &&do_named_START)
#   define STATE(NAME) do_named_ ## NAME :
#   define AUTO_STATE_REF() (&&AUTO_STATE_NAME(__LINE__) - &&do_named_START)
#   define AUTO_STATE() AUTO_STATE_NAME(__LINE__):
#   define BEGIN_DISPATCH goto *(&&do_named_START + writer->state); do_named_START: {

#else

enum ajson_named_states {
    AJSON_STATE_START,
    AJSON_STATE_NUMBER,

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

ssize_t _ajson_write_prelude(ajson_writer *writer, char *buffer, size_t size, size_t index) {
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

ssize_t _ajson_write_dummy(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    (void)ajson_writer_push;
    (void)ajson_writer_pop;
    (void)writer;
    (void)buffer;
    (void)size;
    (void)index;
    return 0;
}

ssize_t _ajson_write_null(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    WRITE_STR("null");

    END_DISPATCH;
}

ssize_t _ajson_write_boolean(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    WRITE_STR(writer->value.boolean ? "true" : "false");

    END_DISPATCH;
}

ssize_t _ajson_write_number(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (isfinite(writer->value.number.value)) {
        size_t nfree = size - index;
        int count = snprintf(((char*)buffer) + index, nfree, "%.16g", writer->value.number.value);
        if (count < 0) {
            RAISE_ERROR();
        }

        writer->value.number.written = nfree;
        if (writer->value.number.written < (size_t)count) {
            index = size;

            CONTINUE(NUMBER);
            STATE(NUMBER);

            char buf[32];
            count = snprintf(buf, sizeof(buf), "%.16g", writer->value.number.value);
            if (count < 0) {
                RAISE_ERROR();
            }
            else if ((size_t)count > sizeof(buf)) {
                errno = ENOBUFS;
                RAISE_ERROR();
            }

            nfree = size - index;
            size_t nrest = (size_t)count - writer->value.number.written;
            if (nfree < nrest) {
                memcpy(((char*)buffer) + index, buf + writer->value.number.written, nfree);
                writer->value.number.written += nfree;
                index = size;
                CONTINUE(NUMBER);
            }
            else {
                memcpy(((char*)buffer) + index, buf + writer->value.number.written, nrest);
                index += nrest;
            }
        }
        else {
            index += count;
        }
    }
    else {
        WRITE_STR("null");
    }

    END_DISPATCH;
}

ssize_t _ajson_write_integer(ajson_writer *writer, char *buffer, size_t size, size_t index) {
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

ssize_t _ajson_write_string(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    WRITE_CHAR('"');

    for (;;) {
        char ch = *writer->value.string;

        if (!ch)
            break;

        if (ch == '"') {
            WRITE_STR("\\\"");
        }
        else if (ch == '\\') {
            WRITE_STR("\\\\");
        }
        else if (ch == '\b') {
            WRITE_STR("\\b");
        }
        else if (ch == '\f') {
            WRITE_STR("\\f");
        }
        else if (ch == '\n') {
            WRITE_STR("\\n");
        }
        else if (ch == '\r') {
            WRITE_STR("\\r");
        }
        else if (ch == '\t') {
            WRITE_STR("\\t");
        }
        else if ((ch >= 0x00 && ch <= 0x1F) || ch == 0x7F) {
            // XXX: does not encode non-ASCII control characters (U+0080–U+009F)
            WRITE_STR("\\u00");

            ch = *writer->value.string >> 4;
            WRITE_CHAR(ch > 9 ? 'a' + ch : '0' + ch);

            ch = *writer->value.string & 0x0F;
            WRITE_CHAR(ch > 9 ? 'a' + ch : '0' + ch);
        }
        else {
            WRITE_CHAR(ch);
        }

        ++ writer->value.string;
    }

    WRITE_CHAR('"');

    END_DISPATCH;
}

ssize_t _ajson_write_begin_array(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (ajson_writer_push(writer, 'A') != 0) {
        RAISE_ERROR();
    }

    WRITE_CHAR('[');

    END_DISPATCH;
}

ssize_t _ajson_write_end_array(ajson_writer *writer, char *buffer, size_t size, size_t index) {
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

ssize_t _ajson_write_begin_object(ajson_writer *writer, char *buffer, size_t size, size_t index) {
    BEGIN_DISPATCH;

    if (ajson_writer_push(writer, 'O') != 0) {
        RAISE_ERROR();
    }

    WRITE_CHAR('{');

    END_DISPATCH;
}

ssize_t _ajson_write_end_object(ajson_writer *writer, char *buffer, size_t size, size_t index) {
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
