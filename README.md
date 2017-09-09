ajson
=====

Asynchronous JSON library written in C.

This library allows you to parse JSON files chunk by chunk, processing only the
data that is available. It does this using a technique similar to
[Protothreads](http://dunkels.com/adam/pt/). This means it records the parse state
in the parser struct and continues the parser function from the point it left it
at the last data chunk. If available it used gcc's
[label as values](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html)
extension to improve the speed of the continuation of the parser function.

**Note:** This library is still of beta quality and interfaces might change in an
incompatible manner. Especially the callback based parsers interface and even the
name of this library (and thus all symbol names) might still change.

Example
-------

```C
ajson_parser parser;

/* possible flags:
 *   AJSON_FLAG_INTEGER
 *        Parse numbers witout decimals or exponent as integers.
 *
 *   AJSON_FLAG_NUMBER_COMPONENTS
 *        Don't combine numbers into doubles, but return their integer, decimal,
 *        and exponent components.
 * 
 *   AJSON_FLAG_NUMBER_AS_STRING
 *        Don't convert numbers into doubles, but instead return it as a strings.
 * 
 *  input encodings:
 *    AJSON_ENC_LATIN1
 *        ISO-8859-1 Technically this is not an encoding that is supposed to be
 *        supported by JSON.
 *
 *    AJSON_ENC_UTF8
 */
if (ajson_init(&parser, AJSON_FLAGS_NONE, AJSON_ENC_UTF8) != 0) {
	perror("initializing JSON parser");
	exit(1);
}

char buffer[BUFSIZ];
for (;;) {
	/* Whether the data is read synchronously or not does not matter to this
	 * library. You just feed it data whenever it becomes available and then
	 * you pull tokens until you get AJSON_TOK_END or AJSON_TOK_NEED_DATA.
	 */
	size_t size = fread(buffer, sizeof(buffer), stream);

	/* Feed a buffer of length 0 to signal the end of the file.
	 *
	 * ajson_feed only fails when there is unparsed data remaining in the parser.
	 * This won't happen if you pull out tokens until you get AJSON_TOK_END or
	 * AJSON_TOK_NEED_DATA.
	 */
	if (ajson_feed(&parser, buffer, size) != 0) {
		perror("feeding data to JSON parser");
		exit(1);
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
			/* If you used AJSON_FLAG_NUMBER_COMPONENTS the number components
			 * will be in parser->value.components.
			 *
			 * If you used AJSON_FLAG_NUMBER_AS_STRING the number will be in
			 * parser->value.string.value as a null ('\0') terminated string.
			 * In addition parser->value.string.length will contain the length
			 * of the string excluding the null byte.
			 * 
			 * Note that parser->value is a union and thus parser->value.number
			 * is not available when one of the flags are used.
			 */
			printf("number: %.16g\n", parser->value.number);
			break;
			
		case AJSON_TOK_INTEGER:
			/* Only happens if you used AJSON_FLAG_INTEGER.
			 */
			printf("integer: %ld\n", parser->value.integer);
			break;

		case AJSON_TOK_STRING:
			/* This token is returned for strings and object keys.
			 *
			 * parser->value.string.value contains the parsed string and is
			 * always null ('\0') terminated. Note that strings might contain
			 * null bytes (by using "\u0000") and thus parser->value.string.length
			 * will provide the actual length of the string (excluding the added
			 * terminating null byte).
			 *
			 * The parsed string is alyways encoded as UTF-8, no matter what the
			 * source encoding was.
			 */
			printf("string: %s\n", parser->value.string.value);
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
			/* Signals the end of the JSON file.
			 */
			printf("end\n");
			has_tokens = false;
			break;

		case AJSON_TOK_ERROR:
			/* parser->input_current will contain the position within the current
			 * chunk of data where the error occured. If you need the absolute
			 * position within your input streem you need to add together all the
			 * sizes of the previous chunks. This might point to the first byte
			 * after the end of the buffer if the error was an unexpected end of
			 * file.
			 */
			printf("error: %s\n", ajson_error_str(parser->value.error.error));
			exit(1);

		case AJSON_TOK_NEED_DATA:
			/* All bytes from the current data chunk are parsed and more is needed.
			 */
			has_tokens = false;
			break;
		}
	}
	
	/* End of file (or IO error, use ferror/feof for a distinction). */
	if (size == 0)
		break;
}

/* Free allocated data structures.
 */
ajson_destroy(&parser);
```

Standard deviations
-------------------

The parser can parse UTF-8 and latin 1 (ISO-8859-1) encoded files. Other encodings
are not supported. Technically ISO-8859-1 is not a valid encoding for JSON files,
but it was very easy to support, so I did it. The writer writes UTF-8 or ASCII files
(ASCII is a subset of UTF-8, so it can always be looked at as UTF-8).

The parser allows strings to contain control characters like null bytes and line
breaks. Thechnically this is invalid JSON but this library allows it anyway. Note
that this might change in the future. The writer will never produce JSON containing
control characters. They will be encoded as e.g. `\u0000` or `\n`.

See Also
--------

[yajl](http://lloyd.github.io/yajl/) is a very very similar library which has a
stable interface and is in wide use.

License
-------

It will probably be LGPL. Nothing more restrictive anyway.
