#!/usr/bin/python

import ctypes
import errno

from ctypes import get_errno
from errno  import EINVAL, ENOMEM
from os     import strerror
from io     import DEFAULT_BUFFER_SIZE

lib = ctypes.CDLL("./build/src/libajson10.so",use_errno=True)

_ajson_init = lib.ajson_init
_ajson_init.argtypes = [ctypes.c_void_p, ctypes.c_int]
_ajson_init.restype  = ctypes.c_int

_ajson_destroy = lib.ajson_destroy
_ajson_destroy.argtypes = [ctypes.c_void_p]
_ajson_destroy.restype  = None

_ajson_feed = lib.ajson_feed
_ajson_feed.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
_ajson_feed.restype  = ctypes.c_int

_ajson_next_token = lib.ajson_next_token
_ajson_feed.argtypes = [ctypes.c_void_p]
_ajson_feed.restype  = ctypes.c_int

_ajson_get_flags = lib.ajson_get_flags
_ajson_get_flags.argtypes = [ctypes.c_void_p]
_ajson_get_flags.restype  = ctypes.c_int

_ajson_get_lineno = lib.ajson_get_lineno
_ajson_get_lineno.argtypes = [ctypes.c_void_p]
_ajson_get_lineno.restype  = ctypes.c_size_t

_ajson_get_columnno = lib.ajson_get_columnno
_ajson_get_columnno.argtypes = [ctypes.c_void_p]
_ajson_get_columnno.restype  = ctypes.c_size_t

_ajson_get_boolean = lib.ajson_get_boolean
_ajson_get_boolean.argtypes = [ctypes.c_void_p]
_ajson_get_boolean.restype  = ctypes.c_bool

_ajson_get_number = lib.ajson_get_number
_ajson_get_number.argtypes = [ctypes.c_void_p]
_ajson_get_number.restype  = ctypes.c_double

_ajson_get_integer = lib.ajson_get_integer
_ajson_get_integer.argtypes = [ctypes.c_void_p]
_ajson_get_integer.restype  = ctypes.c_int64

_ajson_get_string = lib.ajson_get_string
_ajson_get_string.argtypes = [ctypes.c_void_p]
_ajson_get_string.restype  = ctypes.c_char_p

_ajson_get_components_positive = lib.ajson_get_components_positive
_ajson_get_components_positive.argtypes = [ctypes.c_void_p]
_ajson_get_components_positive.restype  = ctypes.c_bool

_ajson_get_components_exponent_positive = lib.ajson_get_components_exponent_positive
_ajson_get_components_exponent_positive.argtypes = [ctypes.c_void_p]
_ajson_get_components_exponent_positive.restype  = ctypes.c_bool

_ajson_get_components_integer = lib.ajson_get_components_integer
_ajson_get_components_integer.argtypes = [ctypes.c_void_p]
_ajson_get_components_integer.restype  = ctypes.c_uint64

_ajson_get_components_decimal = lib.ajson_get_components_decimal
_ajson_get_components_decimal.argtypes = [ctypes.c_void_p]
_ajson_get_components_decimal.restype  = ctypes.c_uint64

_ajson_get_components_decimal_places = lib.ajson_get_components_decimal_places
_ajson_get_components_decimal_places.argtypes = [ctypes.c_void_p]
_ajson_get_components_decimal_places.restype  = ctypes.c_uint64

_ajson_get_components_exponent = lib.ajson_get_components_exponent
_ajson_get_components_exponent.argtypes = [ctypes.c_void_p]
_ajson_get_components_exponent.restype  = ctypes.c_uint64

_ajson_error_str = lib.ajson_error_str
_ajson_error_str.argtypes = [ctypes.c_int]
_ajson_error_str.restype  = ctypes.c_char_p

_ajson_get_error = lib.ajson_get_error
_ajson_get_error.argtypes = [ctypes.c_void_p]
_ajson_get_error.restype  = ctypes.c_int

_ajson_get_error_filename = lib.ajson_get_error_filename
_ajson_get_error_filename.argtypes = [ctypes.c_void_p]
_ajson_get_error_filename.restype  = ctypes.c_char_p

_ajson_get_error_function = lib.ajson_get_error_function
_ajson_get_error_function.argtypes = [ctypes.c_void_p]
_ajson_get_error_function.restype  = ctypes.c_char_p

_ajson_get_error_lineno = lib.ajson_get_error_lineno
_ajson_get_error_lineno.argtypes = [ctypes.c_void_p]
_ajson_get_error_lineno.restype  = ctypes.c_size_t

_ajson_alloc = lib.ajson_alloc
_ajson_alloc.argtypes = [ctypes.c_int]
_ajson_alloc.restype  = ctypes.c_void_p

_ajson_free = lib.ajson_free
_ajson_free.argtypes = [ctypes.c_void_p]
_ajson_free.restype  = None

FLAG_NONE              = 0
FLAG_INTEGER           = 1
FLAG_NUMBER_COMPONENTS = 2

TOK_NEED_DATA    =  0
TOK_NULL         =  1
TOK_BOOLEAN      =  2
TOK_NUMBER       =  3
TOK_INTEGER      =  4
TOK_STRING       =  5
TOK_BEGIN_ARRAY  =  6
TOK_END_ARRAY    =  7
TOK_BEGIN_OBJECT =  8
TOK_END_OBJECT   =  9
TOK_END          = 10
TOK_ERROR        = 11

VALUE_TOKENS = {TOK_NULL, TOK_BOOLEAN, TOK_NUMBER, TOK_INTEGER, TOK_STRING}

ERROR_NONE                       =  0
ERROR_MEMORY                     =  1
ERROR_EMPTY_SATCK                =  2
ERROR_JUMP                       =  3
ERROR_PARSER                     =  4
ERROR_PARSER_UNEXPECTED          =  5
ERROR_PARSER_EXPECTED_ARRAY_END  =  6
ERROR_PARSER_EXPECTED_OBJECT_END =  7
ERROR_PARSER_UNICODE             =  8
ERROR_PARSER_RANGE               =  9
ERROR_PARSER_UNEXPECTED_EOF      = 10

def _error_from_errno():
	err = get_errno()
	if err == INVAL:
		raise ValueError
	elif err == ENOMEM:
		raise MemoryError
	else:
		raise OSError(err,strerror(err))	

class ParserError(Exception):
	__slots__ = 'lineno', 'columnno', 'error', 'source_filename', \
	            'source_lineno', 'source_function', 'message'

	def __init__(self,lineno,columnno,error,source_filename,source_lineno,source_function):
		Exception.__init__(self)

		self.lineno   = lineno
		self.columnno = columnno
		self.error    = error
		self.message  = _ajson_error_str(error).decode('utf-8')
		self.source_filename = source_filename
		self.source_lineno   = source_lineno
		self.source_function = source_function
	
	def __str__(self):
		return "%d:%d: %s\n%s:%s: %s: error raised here" % (
			self.lineno, self.columnno, self.message,
			self.source_filename if self.source_filename is not None else 'N/A',
			self.source_lineno   if self.source_lineno   is not None else 'N/A',
			self.source_function if self.source_function is not None else 'N/A')

class Parser(object):
	__slots__ = '_parser',

	def __init__(self,flags=FLAG_NONE):
		self._parser = _ajson_alloc(flags)
		if not self._parser:
			_error_from_errno()

	def __del__(self):
		_ajson_free(self._parser)

	@property
	def flags(self):
		return _ajson_get_flags(self._parser)

	@property
	def lineno(self):
		return _ajson_get_lineno(self._parser)

	@property
	def columnno(self):
		return _ajson_get_columnno(self._parser)

	def feed(self,data):
		if _ajson_feed(self._parser, data, len(data)) != 0:
			_error_from_errno()
	
	def send(self,data):
		self.feed(data)
		return self.next()

	def next(self):
		ptr   = self._parser
		token = _ajson_next_token(ptr)

		if token == TOK_NEED_DATA    or \
		   token == TOK_BEGIN_ARRAY  or \
		   token == TOK_END_ARRAY    or \
		   token == TOK_BEGIN_OBJECT or \
		   token == TOK_END_OBJECT   or \
		   token == TOK_NULL:
			return token, None

		elif token == TOK_BOOLEAN:
			return token, _ajson_get_boolean(ptr)

		elif token == TOK_NUMBER:
			if _ajson_get_flags(ptr) & FLAG_NUMBER_COMPONENTS:
				return token, (
						_ajson_get_components_positive(ptr),
						_ajson_get_components_integer(ptr),
						_ajson_get_components_decimal(ptr),
						_ajson_get_components_decimal_places(ptr),
						_ajson_get_components_exponent_positive(ptr),
						_ajson_get_components_exponent(ptr)
					)
			else:
				return token, _ajson_get_number(ptr)

		elif token == TOK_INTEGER:
			return token, _ajson_get_integer(ptr)
			
		elif token == TOK_STRING:
			return token, _ajson_get_string(ptr).decode('utf-8')
			
		elif token == TOK_END:
			raise StopIteration

		elif token == TOK_ERROR:
			raise ParserError(
				parser.lineno,
				parser.columnno,
				_ajson_get_error(ptr),
				_ajson_get_error_filename(ptr),
				_ajson_get_error_lineno(ptr),
				_ajson_get_error_function(ptr))

		else:
			raise ValueError("unkown token type: %d" % token)

	def __iter__(self):
		return self

def parse_string(s,flags=FLAG_NONE):
	parser = Parser(flags)
	if type(s) is not bytes:
		s = s.encode('utf-8')
	parser.feed(s)
	for item in parser:
		tok = item[0]
		if tok == TOK_NEED_DATA:
			# signal EOF
			parser.feed(bytes())
		else:
			yield item

def parse_stream(stream,flags=FLAG_NONE):
	parser = Parser(flags)
	for item in parser:
		tok = item[0]
		if tok == TOK_NEED_DATA:
			data = steam.read(DEFAULT_BUFFER_SIZE)
			parser.feed(data)
		else:
			yield item

try:
	xrange
except NameError:
	xrange = range

def _load_values(parser,end_tok):
	for tok, value in parser:
		if tok == end_tok:
			return
		
		elif tok == TOK_BEGIN_ARRAY:
			yield list(_load_values(parser,TOK_END_ARRAY))

		elif tok == TOK_BEGIN_OBJECT:
			values = list(_load_values(parser,TOK_END_OBJECT))
			yield dict((values[i], values[i+1]) for i in xrange(0,len(values),2))

		else:
			assert tok in VALUE_TOKENS, "not a value token: %d" % tok

			yield value

def load(stream,use_int=False):
	parser = parse_stream(stream,FLAG_INTEGER if use_int else FLAG_NONE)
	values = list(_load_values(parser, TOK_END))
	assert len(values) == 1
	return values[0]

def loads(s,use_int=False):
	parser = parse_string(s,FLAG_INTEGER if use_int else FLAG_NONE)
	values = list(_load_values(parser, TOK_END))
	assert len(values) == 1
	return values[0]
