#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>

#include "ajson.h"

int main(int argc, char *argv[]) {
    struct option long_options[] = {
        {"help",              no_argument,       0, 'h'},
        {"integers",          no_argument,       0, 'i'},
        {"number-components", no_argument,       0, 'c'},
        {"numbers-as-string", no_argument,       0, 's'},
        {"encoding",          required_argument, 0, 'e'},
        {0,                   0,                 0,  0 }
    };

    FILE* fp;
    int   flags = AJSON_FLAGS_NONE;
    enum ajson_encoding encoding = AJSON_ENC_UTF8;
    ajson_parser        parser;

    for (;;) {
        int opt = getopt_long(argc, argv, "hie:aI:n", long_options, NULL);

        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
            printf(
                        "usage: %s [options] [input-file]\n"
                        "\n"
                        "\t-h, --help                 print this help message\n"
                        "\t-i, --integer              parse numbers without decimals or exponent as 64bit integers\n"
                        "\t-c, --number-components    print parsed number components instead of constructed floating point number\n"
                        "\t-s, --numbers-as-string    parse numbers as string\n"
                        "\t-e, --encoding=ENCODING    input encoding 'UTF-8' (default) or 'LATIN-1'\n",
                        argc > 0 ? argv[0] : "prettyprint");
            return 0;

        case 'i':
            flags |= AJSON_FLAG_INTEGER;
            break;

        case 'c':
            flags |= AJSON_FLAG_NUMBER_COMPONENTS;
            break;

        case 's':
            flags |= AJSON_FLAG_NUMBER_AS_STRING;
            break;

        case 'e':
            if (strcasecmp(optarg, "UTF-8") == 0 || strcasecmp(optarg, "UTF8") == 0) {
                encoding = AJSON_ENC_UTF8;
            }
            else if (strcasecmp(optarg, "LATIN-1") == 0 || strcasecmp(optarg, "LATIN1") == 0 || strcasecmp(optarg, "ISO-8859-1") == 0 || strcasecmp(optarg, "ISO_8859-1") == 0) {
                encoding = AJSON_ENC_LATIN1;
            }
            else {
                fprintf(stderr, "*** unsupported encoding: %s\n", optarg);
                return 1;
            }
            break;

        case '?':
            fprintf(stderr, "*** unknown option: -%s\n", optarg);
            return 1;
        }
    }

    if (optind < argc) {
        fp = fopen(argv[optind], "r");
        if (!fp) {
            perror(argv[optind]);
            return 1;
        }
    }
    else {
        fp = stdin;
    }

    if (ajson_init(&parser, flags, encoding) != 0) {
        perror("ajson_init");
        if (optind < argc) fclose(fp);
        return 1;
    }

    char buf[BUFSIZ];

    for (;;) {
        size_t size = fgets(buf, sizeof(buf), fp) ? strlen(buf) : 0;

        if (ajson_feed(&parser, buf, size) != 0) {
            perror("ajson_feed");
            ajson_destroy(&parser);
            if (optind < argc) fclose(fp);
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
                if (flags & AJSON_FLAG_NUMBER_COMPONENTS) {
                    printf("TOK: number: isinteger: %s, positive: %s, integer: %" PRIu64 ", decimal: %" PRIu64 ", decimal_places: %" PRIu64 ", exponent_positive: %s, exponent: %" PRIu64 "\n",
                           parser.value.components.isinteger ? "true" : "false",
                           parser.value.components.positive  ? "true" : "false",
                           parser.value.components.integer,
                           parser.value.components.decimal,
                           parser.value.components.decimal_places,
                           parser.value.components.exponent_positive ? "true" : "false",
                           parser.value.components.exponent);
                }
                else if (flags & AJSON_FLAG_NUMBER_AS_STRING) {
                    printf("TOK: number: %s\n", parser.value.string);
                }
                else {
                    printf("TOK: number: %.16g\n", parser.value.number);
                }
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
                fprintf(stderr, "%s: ajson_parse: %s\n", optind < argc ? argv[optind] : "<stdin>", ajson_error_str(parser.value.error.error));
                fprintf(stderr, "%s:%zu: %s: error raised here\n", parser.value.error.filename, parser.value.error.lineno, parser.value.error.function);
                ajson_destroy(&parser);
                if (optind < argc) fclose(fp);
                return 1;

            case AJSON_TOK_NEED_DATA:
                has_tokens = false;
                break;
            }
        }

        if (size == 0)
            break;
    }

    if (ferror(fp)) {
        perror("fread");
        ajson_destroy(&parser);
        if (optind < argc) fclose(fp);
        return 1;
    }

    ajson_destroy(&parser);
    if (optind < argc) fclose(fp);

    return 0;
}
