#include <stdio.h>
#include <string.h>

#include "ajson.h"

int main() {
    ajson_parser parser;

    if (ajson_init(&parser, 0) != 0) {
        perror("ajson_init");
        return 1;
    }

    char buf[2];

    for (;;) {
        size_t size = fgets(buf, sizeof(buf), stdin) ? strlen(buf) : 0;

        if (ajson_feed(&parser, buf, size) != 0) {
            perror("ajson_feed");
            ajson_destroy(&parser);
            return 1;
        }

        bool has_tokens = true;
        while (has_tokens) {
            enum ajson_token token = ajson_next_token(&parser);

            switch (token) {
            case AJSON_TOK_NULL:
                printf("TOK: null\n");
                break;

            case AJSON_TOK_BOOLEAN:
                printf("TOK: boolean: %s\n", parser.value.boolean ? "true" : "false");
                break;

            case AJSON_TOK_NUMBER:
                printf("TOK: number: %lf\n", parser.value.number);
                break;

            case AJSON_TOK_INTEGER:
                printf("TOK: integer: %ld\n", parser.value.integer);
                break;

            case AJSON_TOK_STRING:
                printf("TOK: string: %s\n", parser.value.string);
                break;

            case AJSON_TOK_BEGIN_ARRAY:
                printf("TOK: [\n");
                break;

            case AJSON_TOK_END_ARRAY:
                printf("TOK: ]\n");
                break;

            case AJSON_TOK_BEGIN_OBJECT:
                printf("TOK: {\n");
                break;

            case AJSON_TOK_END_OBJECT:
                printf("TOK: }\n");
                break;

            case AJSON_TOK_END:
                printf("TOK: END\n");
                has_tokens = false;
                break;

            case AJSON_TOK_ERROR:
                printf("TOK: error\n");
                fprintf(stderr, "<stdin>:%zu:%zu: ajson_parse: %s\n", parser.lineno, parser.columnno, ajson_error_str(parser.value.error.error));
                fprintf(stderr, "%s:%zu: %s: error raised here\n", parser.value.error.filename, parser.value.error.lineno, parser.value.error.function);
                ajson_destroy(&parser);
                return 1;

            case AJSON_TOK_NEED_DATA:
                has_tokens = false;
                break;
            }
        }

        if (size == 0)
            break;
    }

    if (ferror(stdin)) {
        perror("fread");
        ajson_destroy(&parser);
        return 1;
    }

    ajson_destroy(&parser);

    return 0;
}
