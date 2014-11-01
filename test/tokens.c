#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>

#include "ajson.h"

enum ajson_read {
    AJSON_READ_FGETS,
    AJSON_READ_FREAD
};

int print_string(const char *string, size_t length) {
    ajson_writer writer;
    if (ajson_writer_init(&writer, AJSON_WRITER_FLAG_ASCII, NULL) != 0) {
        return 1;
    }

    char buf[BUFSIZ];

    ssize_t written = ajson_write_string(&writer, buf, sizeof(buf), string, length, AJSON_ENC_UTF8);

    while (written > 0) {
        if (fwrite(buf, 1, written, stdout) < (size_t)written) {
            perror("fwrite");
            ajson_writer_destroy(&writer);
            return 1;
        }
        if ((size_t)written < sizeof(buf)) break;
        written = ajson_write_continue(&writer, buf, sizeof(buf));
    }

    if (written < 0) {
        perror("ajson_write_*");
        ajson_writer_destroy(&writer);
        return 1;
    }

    ajson_writer_destroy(&writer);
    return 0;
}

int tokenize(FILE* fp, ajson_parser *parser, char *buffer, size_t buffer_size, int flags, enum ajson_read read, bool debug) {
    ajson_reset(parser);

    for (;;) {
        size_t size = read == AJSON_READ_FGETS ?
            (fgets(buffer, buffer_size, fp) ? strlen(buffer) : 0) :
            fread(buffer, 1, buffer_size, fp);

        if (ajson_feed(parser, buffer, size) != 0) {
            perror("ajson_feed");
            return 1;
        }

        bool has_tokens = true;
        while (has_tokens) {
            enum ajson_token token = ajson_next_token(parser);

            switch (token) {
            case AJSON_TOK_NULL:
                printf("null\n");
                break;

            case AJSON_TOK_BOOLEAN:
                printf("boolean: %s\n", parser->value.boolean ? "true" : "false");
                break;

            case AJSON_TOK_NUMBER:
                if (flags & AJSON_FLAG_NUMBER_COMPONENTS) {
                    printf("number: isinteger: %s, positive: %s, integer: %" PRIu64 ", decimal: %" PRIu64 ", decimal_places: %" PRIu64 ", exponent_positive: %s, exponent: %" PRIu64 "\n",
                           parser->value.components.isinteger ? "true" : "false",
                           parser->value.components.positive  ? "true" : "false",
                           parser->value.components.integer,
                           parser->value.components.decimal,
                           parser->value.components.decimal_places,
                           parser->value.components.exponent_positive ? "true" : "false",
                           parser->value.components.exponent);
                }
                else if (flags & AJSON_FLAG_NUMBER_AS_STRING) {
                    printf("number: ");
                    if (print_string(parser->value.string.value, parser->value.string.length) != 0) {
                        return 1;
                    }
                    printf("\n");
                }
                else {
                    printf("number: %.16g\n", parser->value.number);
                }
                break;

            case AJSON_TOK_INTEGER:
                printf("integer: %ld\n", parser->value.integer);
                break;

            case AJSON_TOK_STRING:
                printf("string: ");
                if (print_string(parser->value.string.value, parser->value.string.length) != 0) {
                    return 1;
                }
                printf("\n");
                break;

            case AJSON_TOK_BEGIN_ARRAY:
                printf("[\n");
                break;

            case AJSON_TOK_END_ARRAY:
                printf("]\n");
                break;

            case AJSON_TOK_BEGIN_OBJECT:
                printf("{\n");
                break;

            case AJSON_TOK_END_OBJECT:
                printf("}\n");
                break;

            case AJSON_TOK_END:
                printf("end\n");
                has_tokens = false;
                break;

            case AJSON_TOK_ERROR:
                printf("error: (%d) %s\n", parser->value.error.error, ajson_error_str(parser->value.error.error));
                if (debug) {
                    fprintf(stderr, "%s:%zu: %s: error raised here\n",
                            parser->value.error.filename,
                            parser->value.error.lineno,
                            parser->value.error.function);
                }
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
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct option long_options[] = {
        {"help",              no_argument,       0, 'h'},
        {"integers",          no_argument,       0, 'i'},
        {"number-components", no_argument,       0, 'c'},
        {"numbers-as-string", no_argument,       0, 's'},
        {"encoding",          required_argument, 0, 'e'},
        {"buffer-size",       required_argument, 0, 'b'},
        {"read",              required_argument, 0, 'r'},
        {"debug",             no_argument,       0, 'd'},
        {0,                   0,                 0,  0 }
    };

    int  status = 0;
    int  flags  = AJSON_FLAGS_NONE;
    bool debug  = false;
    enum ajson_encoding encoding = AJSON_ENC_UTF8;
    ajson_parser        parser;
    bool                parser_needs_freeing = false;
    size_t              buffer_size = BUFSIZ;
    char*               buffer      = NULL;
    enum ajson_read     read        = AJSON_READ_FREAD;

    for (;;) {
        int opt = getopt_long(argc, argv, "hie:aI:nb:r:d", long_options, NULL);

        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
            printf(
                        "usage: %s [options] [input-file]...\n"
                        "\n"
                        "OPTIONS:\n"
                        "\t-h, --help                 print this help message\n"
                        "\t-i, --integer              parse numbers without decimals or exponent as 64bit integers\n"
                        "\t-c, --number-components    print parsed number components instead of constructed floating point number\n"
                        "\t-s, --numbers-as-string    parse numbers as string\n"
                        "\t-e, --encoding=ENCODING    input encoding: 'UTF-8' (default) or 'LATIN-1'\n"
                        "\t-b, --buffer-size=SIZE     size of read buffer in bytes (default: %d)\n"
                        "\t-r, --read=METHOD          read method: 'fread' (default) or 'fgets'\n"
                        "\t-d, --debug                print C source line of error\n",
                        argc > 0 ? argv[0] : "tokens", BUFSIZ);
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

        case 'b':
        {
            char *endptr = NULL;
            buffer_size = strtoul(optarg, &endptr, 10);
            if (*endptr || buffer_size == 0) {
                fprintf(stderr, "*** invalid buffer size: %s\n", optarg);
                return 1;
            }
            break;
        }
        case 'r':
            if (strcasecmp(optarg, "fgets") == 0) {
                read = AJSON_READ_FGETS;
            }
            else if (strcasecmp(optarg, "fread") == 0) {
                read = AJSON_READ_FREAD;
            }
            else {
                fprintf(stderr, "*** invalid read method: %s\n", optarg);
                return 1;
            }
            break;

        case 'd':
            debug = true;
            break;

        case '?':
            fprintf(stderr, "*** unknown option: -%s\n", optarg);
            return 1;
        }
    }

    // +1 for nil
    if (read == AJSON_READ_FGETS) {
        if (buffer_size == SIZE_MAX) {
            fprintf(stderr, "*** buffer size to big: %zu\n", buffer_size);
            return 1;
        }
        ++ buffer_size;
    }


    buffer = malloc(buffer_size);
    if (!buffer) {
        perror("malloc");
        status = 1;
        goto cleanup;
    }

    if (ajson_init(&parser, flags, encoding) != 0) {
        perror("ajson_init");
        status = 1;
        goto cleanup;
    }
    parser_needs_freeing = true;

    if (optind < argc) {
        for (; optind < argc; ++ optind) {
            FILE *fp = fopen(argv[optind], "rb");

            if (!fp) {
                perror(argv[optind]);
                status = 1;
                goto cleanup;
            }

            status = tokenize(fp, &parser, buffer, buffer_size, flags, read, debug);

            fclose(fp);

            if (status != 0)
                break;
        }
    }
    else {
        status = tokenize(stdin, &parser, buffer, buffer_size, flags, read, debug);
    }

cleanup:
    if (buffer) {
        free(buffer);
    }

    if (parser_needs_freeing) {
        ajson_destroy(&parser);
    }

    return status;
}
