#include "ajson.h"

#include <stdio.h>
#include <string.h>

int main(int argc, const char *argv[]) {
    FILE*        fp;
    ajson_parser parser;
    ajson_writer writer;

    if (argc > 1) {
        fp = fopen(argv[1], "r");
        if (!fp) {
            perror(argv[1]);
            return 1;
        }
    }
    else {
        fp = stdin;
    }

    if (ajson_init(&parser, AJSON_FLAG_INTEGER) != 0) {
        perror("ajson_init");
        if (argc > 1) fclose(fp);
        return 1;
    }

    if (ajson_writer_init(&writer, AJSON_WRITER_FLAGS_NONE, "\t") != 0) {
        perror("ajson_writer_init");
        if (argc > 1) fclose(fp);
        ajson_destroy(&parser);
        return 1;
    }

    char inbuf[BUFSIZ];
    char outbuf[2];

    for (;;) {
        size_t size = fgets(inbuf, sizeof(inbuf), fp) ? strlen(inbuf) : 0;

        if (ajson_feed(&parser, inbuf, size) != 0) {
            perror("ajson_feed");
            if (argc > 1) fclose(fp);
            ajson_writer_destroy(&writer);
            ajson_destroy(&parser);
            return 1;
        }

        bool has_tokens = true;
        while (has_tokens) {
            enum ajson_token token = ajson_next_token(&parser);
            ssize_t written = 0;

            switch (token) {
            case AJSON_TOK_NULL:
                written = ajson_write_null(&writer, outbuf, sizeof(outbuf));
                break;

            case AJSON_TOK_BOOLEAN:
                written = ajson_write_boolean(&writer, outbuf, sizeof(outbuf), parser.value.boolean);
                break;

            case AJSON_TOK_NUMBER:
                written = ajson_write_number(&writer, outbuf, sizeof(outbuf), parser.value.number);
                break;

            case AJSON_TOK_INTEGER:
                written = ajson_write_integer(&writer, outbuf, sizeof(outbuf), parser.value.integer);
                break;

            case AJSON_TOK_STRING:
                written = ajson_write_string_utf8(&writer, outbuf, sizeof(outbuf), parser.value.string);
                break;

            case AJSON_TOK_BEGIN_ARRAY:
                written = ajson_write_begin_array(&writer, outbuf, sizeof(outbuf));
                break;

            case AJSON_TOK_END_ARRAY:
                written = ajson_write_end_array(&writer, outbuf, sizeof(outbuf));
                break;

            case AJSON_TOK_BEGIN_OBJECT:
                written = ajson_write_begin_object(&writer, outbuf, sizeof(outbuf));
                break;

            case AJSON_TOK_END_OBJECT:
                written = ajson_write_end_object(&writer, outbuf, sizeof(outbuf));
                break;

            case AJSON_TOK_END:
                has_tokens = false;
                putchar('\n');
                break;

            case AJSON_TOK_ERROR:
                fprintf(stderr, "%s:%zu:%zu: ajson_parse: %s\n", argc > 1 ? argv[1] : "<stdin>", parser.lineno, parser.columnno, ajson_error_str(parser.value.error.error));
                fprintf(stderr, "%s:%zu: %s: error raised here\n", parser.value.error.filename, parser.value.error.lineno, parser.value.error.function);

                if (argc > 1) fclose(fp);
                ajson_writer_destroy(&writer);
                ajson_destroy(&parser);
                return 1;

            case AJSON_TOK_NEED_DATA:
                has_tokens = false;
                break;
            }

            while (written > 0) {
                if (fwrite(outbuf, 1, written, stdout) < (size_t)written) {
                    perror("fwrite");
                    if (argc > 1) fclose(fp);
                    ajson_writer_destroy(&writer);
                    ajson_destroy(&parser);
                    return 1;
                }
                if ((size_t)written < sizeof(outbuf)) break;
                written = ajson_write_continue(&writer, outbuf, sizeof(outbuf));
            }

            if (written < 0) {
                perror("ajson_write_*");
                if (argc > 1) fclose(fp);
                ajson_writer_destroy(&writer);
                ajson_destroy(&parser);
                return 1;
            }
        }

        if (size == 0)
            break;
    }

    if (argc > 1) fclose(fp);
    ajson_writer_destroy(&writer);
    ajson_destroy(&parser);

    return 0;
}
