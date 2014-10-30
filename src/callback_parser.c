#include "ajson.h"

#include <unistd.h>

int ajson_cb_dispatch(ajson_cb_parser *parser) {
    for (;;) {
        enum ajson_token token = ajson_next_token(&parser->parser);

        switch (token) {
        case AJSON_TOK_NULL:
            if (parser->null_func)
                parser->null_func(parser->ctx);
            break;

        case AJSON_TOK_BOOLEAN:
            if (parser->boolean_func)
                parser->boolean_func(parser->ctx, parser->parser.value.boolean);
            break;

        case AJSON_TOK_NUMBER:
            if (parser->parser.flags & AJSON_FLAG_NUMBER_COMPONENTS) {
                if (parser->components_func)
                    parser->components_func(parser->ctx,
                                            parser->parser.value.components.positive,
                                            parser->parser.value.components.integer,
                                            parser->parser.value.components.decimal,
                                            parser->parser.value.components.decimal_places,
                                            parser->parser.value.components.exponent_positive,
                                            parser->parser.value.components.exponent);
            }
            else {
                if (parser->number_func)
                    parser->number_func(parser->ctx, parser->parser.value.number);
            }
            break;

        case AJSON_TOK_INTEGER:
            if (parser->integer_func)
                parser->integer_func(parser->ctx, parser->parser.value.integer);
            break;

        case AJSON_TOK_STRING:
            if (parser->string_func)
                parser->string_func(parser->ctx, parser->parser.value.string.value, parser->parser.value.string.length);
            break;

        case AJSON_TOK_BEGIN_ARRAY:
            if (parser->begin_array_func)
                parser->begin_array_func(parser->ctx);
            break;

        case AJSON_TOK_END_ARRAY:
            if (parser->end_array_func)
                parser->end_array_func(parser->ctx);
            break;

        case AJSON_TOK_BEGIN_OBJECT:
            if (parser->begin_object_func)
                parser->begin_object_func(parser->ctx);
            break;

        case AJSON_TOK_END_OBJECT:
            if (parser->end_object_func)
                parser->end_object_func(parser->ctx);
            break;

        case AJSON_TOK_END:
            if (parser->end_func)
                parser->end_func(parser->ctx);
            return 0;

        case AJSON_TOK_ERROR:
            if (parser->error_func)
                parser->error_func(parser->ctx, parser->parser.value.error.error);
            return -1;

        case AJSON_TOK_NEED_DATA:
            return 0;
        }
    }
}

int ajson_cb_parse_fd(ajson_cb_parser *parser, int fd) {
    char buf[BUFSIZ];

    for (;;) {
        ssize_t count = read(fd, buf, sizeof(buf));

        if (count < 0 || ajson_feed(&parser->parser, buf, count) != 0 || ajson_cb_dispatch(parser) != 0) {
            return -1;
        }

        if (count == 0)
            break;
    }

    return 0;
}

int ajson_cb_parse_file(ajson_cb_parser *parser, FILE* stream) {
    char buf[BUFSIZ];

    for (;;) {
        size_t count = fread(buf, 1, sizeof(buf), stream);

        if (ferror(stream) || ajson_feed(&parser->parser, buf, count) != 0 || ajson_cb_dispatch(parser) != 0) {
            return -1;
        }

        if (count == 0)
            break;
    }

    return 0;

}

int ajson_cb_parse_buf(ajson_cb_parser *parser, const void* buffer, size_t size) {
    if (ajson_feed(&parser->parser, buffer, size) != 0) {
        return -1;
    }
    return ajson_cb_dispatch(parser);
}
