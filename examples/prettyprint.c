#include "ajson.h"

#include <stdio.h>
#include <string.h>
#include <getopt.h>

enum ajson_read {
    AJSON_READ_FGETS,
    AJSON_READ_FREAD
};

int main(int argc, char *argv[]) {
    struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"integers", no_argument,       0, 'i'},
        {"encoding", required_argument, 0, 'e'},
        {"ascii",    no_argument,       0, 'a'},
        {"indent",   required_argument, 0, 'I'},
        {"ugly",     no_argument,       0, 'u'},
        {"read",     required_argument, 0, 'r'},
        {0,          0,                 0,  0 }
    };

    FILE*        fp;
    int          parser_flags = AJSON_FLAGS_NONE;
    int          writer_flags = AJSON_WRITER_FLAGS_NONE;
    const char*  indent = "\t";
    enum ajson_encoding encoding = AJSON_ENC_UTF8;
    enum ajson_read     read     = AJSON_READ_FREAD;
    ajson_parser parser;
    ajson_writer writer;

    for (;;) {
        int opt = getopt_long(argc, argv, "hie:aI:n", long_options, NULL);

        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
            printf(
                        "usage: %s [options] [input-file]\n"
                        "\n"
                        "OPTIONS:\n"
                        "\t-h, --help                 print this help message\n"
                        "\t-i, --integer              parse numbers without decimals or exponent as 64bit integers\n"
                        "\t-e, --encoding=ENCODING    input encoding 'UTF-8' (default) or 'LATIN-1'\n"
                        "\t-a, --ascii                produce ASCII compatible output\n"
                        "\t-I, --indent=INDENT        use INDENT as indentation (default: $'\\t')\n"
                        "\t-u, --ugly                 don't pretty print\n"
                        "\t-r, --read=METHOD          read method: 'fread' (default) or 'fgets'\n",
                        argc > 0 ? argv[0] : "prettyprint");
            return 0;

        case 'i':
            parser_flags |= AJSON_FLAG_INTEGER;
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

        case 'a':
            writer_flags |= AJSON_WRITER_FLAG_ASCII;
            break;

        case 'I':
            indent = optarg;
            break;

        case 'u':
            indent = NULL;
            break;

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

    if (ajson_init(&parser, parser_flags, encoding) != 0) {
        perror("ajson_init");
        if (optind < argc) fclose(fp);
        return 1;
    }

    if (ajson_writer_init(&writer, writer_flags, indent) != 0) {
        perror("ajson_writer_init");
        if (optind < argc) fclose(fp);
        ajson_destroy(&parser);
        return 1;
    }

    char inbuf[BUFSIZ];
    char outbuf[BUFSIZ];

    for (;;) {
        size_t size = read == AJSON_READ_FGETS ?
            (fgets(inbuf, sizeof(inbuf), fp) ? strlen(inbuf) : 0) :
            fread(inbuf, 1, sizeof(inbuf), fp);

        if (ajson_feed(&parser, inbuf, size) != 0) {
            perror("ajson_feed");
            if (optind < argc) fclose(fp);
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
                written = ajson_write_string(&writer, outbuf, sizeof(outbuf), parser.value.string.value, parser.value.string.length, AJSON_ENC_UTF8);
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
                fprintf(stderr, "%s: ajson_parse: %s\n", optind < argc ? argv[optind] : "<stdin>", ajson_error_str(parser.value.error.error));
                fprintf(stderr, "%s:%zu: %s: error raised here\n", parser.value.error.filename, parser.value.error.lineno, parser.value.error.function);

                if (optind < argc) fclose(fp);
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
                    if (optind < argc) fclose(fp);
                    ajson_writer_destroy(&writer);
                    ajson_destroy(&parser);
                    return 1;
                }
                if ((size_t)written < sizeof(outbuf)) break;
                written = ajson_write_continue(&writer, outbuf, sizeof(outbuf));
            }

            if (written < 0) {
                perror("ajson_write_*");
                if (optind < argc) fclose(fp);
                ajson_writer_destroy(&writer);
                ajson_destroy(&parser);
                return 1;
            }
        }

        if (size == 0)
            break;
    }

    if (optind < argc) fclose(fp);
    ajson_writer_destroy(&writer);
    ajson_destroy(&parser);

    return 0;
}
