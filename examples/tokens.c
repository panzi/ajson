#include <stdio.h>

#include "ajson.h"

int main() {
    ajson_parser parser;

    if (ajson_init(&parser, 0) != 0) {
        perror("ajson_init");
        return 1;
    }

    char buf[BUFSIZ];

    for (;;) {
        size_t size = fread(buf, 1, BUFSIZ, stdin);

        if (ajson_feed(&parser, buf, size) != 0) {
            perror("ajson_feed");
            ajson_destroy(&parser);
            return 1;
        }

        enum ajson_token token;
        while ((token = ajson_next_token(&parser)) != AJSON_TOK_EOF) {
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

            case AJSON_TOK_OBJECT_KEY:
                printf("TOK: key: %s\n", parser.value.string);
                break;

            case AJSON_TOK_END_OBJECT:
                printf("TOK: }\n");
                break;

            case AJSON_TOK_ERROR:
                printf("TOK: error\n");
                fprintf(stderr, "%zu:%zu: ajson_parse: %s\n", parser.lineno, parser.columnno, ajson_error_str(parser.value.error));
                ajson_destroy(&parser);
                return 1;

            case AJSON_TOK_EOF:
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
