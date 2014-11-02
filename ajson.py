#!/usr/bin/python

import ctypes
import errno

from functools import wraps
from ctypes    import get_errno
from os.path   import split as pathsplit, join as pathjoin
from errno     import EINVAL, ENOMEM
from os        import strerror
from io        import DEFAULT_BUFFER_SIZE

__all__ = (
	'ERROR_EMPTY_SATCK', 'ERROR_JUMP', 'ERROR_MEMORY', 'ERROR_NONE', 'ERROR_PARSER',
	'ERROR_PARSER_EXPECTED_ARRAY_END', 'ERROR_PARSER_EXPECTED_OBJECT_END',
	'ERROR_PARSER_RANGE', 'ERROR_PARSER_UNEXPECTED', 'ERROR_PARSER_UNEXPECTED_EOF',
	'ERROR_PARSER_UNICODE',

	'FLAGS_ALL', 'FLAGS_NONE', 'FLAG_INTEGER', 'FLAG_NUMBER_AS_STRING',
	'FLAG_NUMBER_COMPONENTS',

	'TOK_BEGIN_ARRAY', 'TOK_BEGIN_OBJECT', 'TOK_BOOLEAN', 'TOK_END', 'TOK_END_ARRAY',
	'TOK_END_OBJECT', 'TOK_ERROR', 'TOK_INTEGER', 'TOK_NEED_DATA', 'TOK_NULL', 'TOK_NUMBER',
	'TOK_STRING',

	'VALUE_TOKENS',
	'VERSION',

	'WRITER_FALG_ASCII', 'WRITER_FLAGS_ALL', 'WRITER_FLAGS_NONE',

	'FileWriter', 'Parser', 'ParserError', 'Writer',

	'dump', 'dumpb', 'dumps', 'load', 'loadb', 'loads', 'parse_bytes', 'parse_stream',
	'parse_string'
)

try:
	xrange
except NameError:
	xrange = range

_Strings = {str, bytes, memoryview}

try:
	unicode
except NameError:
	pass
else:
	_Strings.add(unicode)

try:
	buffer
except NameError:
	pass
else:
	_Strings.add(buffer)

_Strings = tuple(_Strings)

_Ints = {int}

try:
	long
except NameError:
	pass
else:
	_Ints.add(long)

_Ints = tuple(_Ints)
_Lists = (list, tuple)

ENC_LATIN1 = 0
ENC_UTF8   = 1

FLAG_INTEGER           = 1
FLAG_NUMBER_COMPONENTS = 2
FLAG_NUMBER_AS_STRING  = 4

FLAGS_NONE = 0
FLAGS_ALL  = FLAG_INTEGER | FLAG_NUMBER_COMPONENTS | FLAG_NUMBER_AS_STRING

WRITER_FALG_ASCII = 1

WRITER_FLAGS_NONE = 0
WRITER_FLAGS_ALL  = WRITER_FALG_ASCII

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

ERROR_NONE                                =  0
ERROR_MEMORY                              =  1
ERROR_EMPTY_SATCK                         =  2
ERROR_JUMP                                =  3
ERROR_PARSER_STATE                        =  4
ERROR_PARSER_EXPECTED_DIGIT               =  5
ERROR_PARSER_EXPECTED_HEX                 =  6
ERROR_PARSER_EXPECTED_COMMA_OR_ARRAY_END  =  7
ERROR_PARSER_EXPECTED_COMMA_OR_OBJECT_END =  8
ERROR_PARSER_EXPECTED_STRING              =  9
ERROR_PARSER_EXPECTED_COLON               = 10
ERROR_PARSER_ILLEGAL_ESCAPE               = 11
ERROR_PARSER_ILLEGAL_UNICODE              = 12
ERROR_PARSER_RANGE                        = 13
ERROR_PARSER_UNEXPECTED_CHAR              = 14
ERROR_PARSER_UNEXPECTED_EOF               = 15

def _error_from_errno():
	err = get_errno()
	if err == EINVAL:
		raise ValueError
	elif err == ENOMEM:
		raise MemoryError
	else:
		raise OSError(err,strerror(err))	

class ParserError(Exception):
	__slots__ = 'error', 'filename', \
	            'lineno', 'function', 'message'

	def __init__(self,error,filename,lineno,function):
		Exception.__init__(self)

		self.error    = error
		self.message  = _ajson_error_str(error).decode('utf-8')
		self.filename = filename
		self.lineno   = lineno
		self.function = function

	def __str__(self):
		return "%s\n%s:%s: %s: error raised here" % (
			self.message,
			self.filename if self.filename is not None else 'N/A',
			self.lineno   if self.lineno   is not None else 'N/A',
			self.function if self.function is not None else 'N/A')

class Parser(object):
	__slots__ = '_parser', '_data'

	def __init__(self,flags=FLAGS_NONE):
		self._parser = _ajson_alloc(flags, ENC_UTF8)
		self._data   = None
		if not self._parser:
			_error_from_errno()

	def __del__(self):
		_ajson_free(self._parser)

	def reset(self):
		_ajson_reset(self._parser)

	@property
	def flags(self):
		return _ajson_get_flags(self._parser)

	def feed(self,data):
		if _ajson_feed(self._parser, data, len(data)) != 0:
			_error_from_errno()
		self._data = data # keep reference to data so it won't get GCed

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
			flags = _ajson_get_flags(ptr)
			if flags & FLAG_NUMBER_COMPONENTS:
				return token, (
						_ajson_get_components_positive(ptr),
						_ajson_get_components_integer(ptr),
						_ajson_get_components_decimal(ptr),
						_ajson_get_components_decimal_places(ptr),
						_ajson_get_components_exponent_positive(ptr),
						_ajson_get_components_exponent(ptr)
					)
			elif flags & FLAG_NUMBER_AS_STRING:
				return token, _ajson_get_string(ptr)[:_ajson_get_string_length(ptr)].decode('utf-8')
			else:
				return token, _ajson_get_number(ptr)

		elif token == TOK_INTEGER:
			return token, _ajson_get_integer(ptr)

		elif token == TOK_STRING:
			return token, _ajson_get_string(ptr)[:_ajson_get_string_length(ptr)].decode('utf-8')

		elif token == TOK_END:
			raise StopIteration

		elif token == TOK_ERROR:
			raise ParserError(
				_ajson_get_error(ptr),
				_ajson_get_error_filename(ptr),
				_ajson_get_error_lineno(ptr),
				_ajson_get_error_function(ptr))

		else:
			raise ValueError("unkown token type: %d" % token)

	__next__ = next

	def __iter__(self):
		return self

def parse_bytes(b,flags=FLAGS_NONE):
	parser = Parser(flags)
	parser.feed(b)
	for item in parser:
		tok = item[0]
		if tok == TOK_NEED_DATA:
			# signal EOF
			parser.feed(bytes())
		else:
			yield item

def parse_string(s,flags=FLAGS_NONE):
	return parse_bytes(s.encode('utf-8'),flags)

def parse_stream(stream,flags=FLAGS_NONE):
	parser = Parser(flags)
	for item in parser:
		tok = item[0]
		if tok == TOK_NEED_DATA:
			data = steam.read(DEFAULT_BUFFER_SIZE)
			parser.feed(data)
		else:
			yield item

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
			assert tok in VALUE_TOKENS, "illegal token: %d" % tok

			yield value

def load(stream,use_int=False):
	parser = parse_stream(stream,FLAG_INTEGER if use_int else FLAGS_NONE)
	values = list(_load_values(parser, TOK_END))
	assert len(values) == 1
	return values[0]

def loads(s,use_int=False):
	parser = parse_string(s,FLAG_INTEGER if use_int else FLAGS_NONE)
	values = list(_load_values(parser, TOK_END))
	assert len(values) == 1
	return values[0]

def loadb(b,use_int=False):
	parser = parse_bytes(b,FLAG_INTEGER if use_int else FLAGS_NONE)
	values = list(_load_values(parser, TOK_END))
	assert len(values) == 1
	return values[0]

class _Parser(ctypes.Structure):
	pass

class _Writer(ctypes.Structure):
	pass

_ParserPtr = ctypes.POINTER(_Parser)
_WriterPtr = ctypes.POINTER(_Writer)

# load C functions from shared object
try:
	_lib = ctypes.CDLL(pathjoin(pathsplit(__file__)[0],"libajson.so"),use_errno=True)
except OSError:
	_lib = ctypes.CDLL("libajson.so",use_errno=True)

_ajson_version_major = _lib.ajson_version_major
_ajson_version_major.argtypes = []
_ajson_version_major.restype  = ctypes.c_uint

_ajson_version_minor = _lib.ajson_version_minor
_ajson_version_minor.argtypes = []
_ajson_version_minor.restype  = ctypes.c_uint

_ajson_version_patch = _lib.ajson_version_patch
_ajson_version_patch.argtypes = []
_ajson_version_patch.restype  = ctypes.c_uint

_ajson_alloc = _lib.ajson_alloc
_ajson_alloc.argtypes = [ctypes.c_int, ctypes.c_int]
_ajson_alloc.restype  = _ParserPtr

_ajson_reset = _lib.ajson_reset
_ajson_reset.argtypes = [_ParserPtr]
_ajson_reset.restype  = None

_ajson_free = _lib.ajson_free
_ajson_free.argtypes = [_ParserPtr]
_ajson_free.restype  = None

_ajson_feed = _lib.ajson_feed
_ajson_feed.argtypes = [_ParserPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_feed.restype  = ctypes.c_int

_ajson_next_token = _lib.ajson_next_token
_ajson_next_token.argtypes = [_ParserPtr]
_ajson_next_token.restype  = ctypes.c_int

_ajson_get_flags = _lib.ajson_get_flags
_ajson_get_flags.argtypes = [_ParserPtr]
_ajson_get_flags.restype  = ctypes.c_int

_ajson_get_boolean = _lib.ajson_get_boolean
_ajson_get_boolean.argtypes = [_ParserPtr]
_ajson_get_boolean.restype  = ctypes.c_bool

_ajson_get_number = _lib.ajson_get_number
_ajson_get_number.argtypes = [_ParserPtr]
_ajson_get_number.restype  = ctypes.c_double

_ajson_get_integer = _lib.ajson_get_integer
_ajson_get_integer.argtypes = [_ParserPtr]
_ajson_get_integer.restype  = ctypes.c_int64

_ajson_get_string = _lib.ajson_get_string
_ajson_get_string.argtypes = [_ParserPtr]
_ajson_get_string.restype  = ctypes.POINTER(ctypes.c_char)

_ajson_get_string_length = _lib.ajson_get_string_length
_ajson_get_string_length.argtypes = [_ParserPtr]
_ajson_get_string_length.restype  = ctypes.c_size_t

_ajson_get_components_positive = _lib.ajson_get_components_positive
_ajson_get_components_positive.argtypes = [_ParserPtr]
_ajson_get_components_positive.restype  = ctypes.c_bool

_ajson_get_components_exponent_positive = _lib.ajson_get_components_exponent_positive
_ajson_get_components_exponent_positive.argtypes = [_ParserPtr]
_ajson_get_components_exponent_positive.restype  = ctypes.c_bool

_ajson_get_components_integer = _lib.ajson_get_components_integer
_ajson_get_components_integer.argtypes = [_ParserPtr]
_ajson_get_components_integer.restype  = ctypes.c_uint64

_ajson_get_components_decimal = _lib.ajson_get_components_decimal
_ajson_get_components_decimal.argtypes = [_ParserPtr]
_ajson_get_components_decimal.restype  = ctypes.c_uint64

_ajson_get_components_decimal_places = _lib.ajson_get_components_decimal_places
_ajson_get_components_decimal_places.argtypes = [_ParserPtr]
_ajson_get_components_decimal_places.restype  = ctypes.c_uint64

_ajson_get_components_exponent = _lib.ajson_get_components_exponent
_ajson_get_components_exponent.argtypes = [_ParserPtr]
_ajson_get_components_exponent.restype  = ctypes.c_uint64

_ajson_error_str = _lib.ajson_error_str
_ajson_error_str.argtypes = [ctypes.c_int]
_ajson_error_str.restype  = ctypes.c_char_p

_ajson_get_error = _lib.ajson_get_error
_ajson_get_error.argtypes = [_ParserPtr]
_ajson_get_error.restype  = ctypes.c_int

_ajson_get_error_filename = _lib.ajson_get_error_filename
_ajson_get_error_filename.argtypes = [_ParserPtr]
_ajson_get_error_filename.restype  = ctypes.c_char_p

_ajson_get_error_function = _lib.ajson_get_error_function
_ajson_get_error_function.argtypes = [_ParserPtr]
_ajson_get_error_function.restype  = ctypes.c_char_p

_ajson_get_error_lineno = _lib.ajson_get_error_lineno
_ajson_get_error_lineno.argtypes = [_ParserPtr]
_ajson_get_error_lineno.restype  = ctypes.c_size_t

_ajson_writer_alloc = _lib.ajson_writer_alloc
_ajson_writer_alloc.argtypes = [ctypes.c_int, ctypes.c_char_p]
_ajson_writer_alloc.restype  = _WriterPtr

_ajson_writer_free = _lib.ajson_writer_free
_ajson_writer_free.argtypes = [_WriterPtr]
_ajson_writer_free.restype  = None

_ajson_write_null = _lib.ajson_write_null
_ajson_write_null.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_write_null.restype  = ctypes.c_ssize_t

_ajson_write_boolean = _lib.ajson_write_boolean
_ajson_write_boolean.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_bool]
_ajson_write_boolean.restype  = ctypes.c_ssize_t

_ajson_write_number = _lib.ajson_write_number
_ajson_write_number.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double]
_ajson_write_number.restype  = ctypes.c_ssize_t

_ajson_write_integer = _lib.ajson_write_integer
_ajson_write_integer.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int64]
_ajson_write_integer.restype  = ctypes.c_ssize_t

_ajson_write_string = _lib.ajson_write_string
_ajson_write_string.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t, ctypes.c_int]
_ajson_write_string.restype  = ctypes.c_ssize_t

_ajson_write_begin_array = _lib.ajson_write_begin_array
_ajson_write_begin_array.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_write_begin_array.restype  = ctypes.c_ssize_t

_ajson_write_end_array = _lib.ajson_write_end_array
_ajson_write_end_array.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_write_end_array.restype  = ctypes.c_ssize_t

_ajson_write_begin_object = _lib.ajson_write_begin_object
_ajson_write_begin_object.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_write_begin_object.restype  = ctypes.c_ssize_t

_ajson_write_end_object = _lib.ajson_write_end_object
_ajson_write_end_object.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_write_end_object.restype  = ctypes.c_ssize_t

_ajson_write_continue = _lib.ajson_write_continue
_ajson_write_continue.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_write_continue.restype  = ctypes.c_ssize_t

_ajson_writer_get_flags = _lib.ajson_writer_get_flags
_ajson_writer_get_flags.argtypes = [_WriterPtr]
_ajson_writer_get_flags.restype  = ctypes.c_int

def _make_write_func(write_func):
	@wraps(write_func)
	def _write_func(self,*args):
		ptr    = self._writer
		buf    = self._buffer
		bufsiz = len(buf)
		size   = write_func(ptr, buf, bufsiz, *args)

		while size == bufsiz:
			yield buf.raw

			size = _ajson_write_continue(ptr, buf, bufsiz)

		if size < 0:
			_error_from_errno()

		if size > 0:
			yield buf[:size]

	return _write_func

class WriterBase(object):
	__slots__ = '_writer', '_indent', '_buffer'

	def __init__(self,flags=WRITER_FLAGS_NONE,indent=None,buffer_size=DEFAULT_BUFFER_SIZE):
		if buffer_size < 1:
			raise ValueError("buffer_size musst be bigger than zero")
		# keep reference to indent so it won't get GCed
		self._indent = None if indent is None else indent.encode('utf-8')
		self._buffer = ctypes.create_string_buffer(buffer_size)
		self._writer = _ajson_writer_alloc(flags,self._indent)
		if not self._writer:
			_error_from_errno()

	def __del__(self):
		_ajson_writer_free(self._writer)

	@property
	def buffer_size(self):
		return len(self._buffer)

	@property
	def indent(self):
		return self._indent.decode('utf-8')

	@property
	def flags(self):
		return _ajson_writer_get_flags(self._writer)

class Writer(WriterBase):
	__slots__ = ()

	null         = _make_write_func(_ajson_write_null)
	boolean      = _make_write_func(_ajson_write_boolean)
	number       = _make_write_func(_ajson_write_number)
	integer      = _make_write_func(_ajson_write_integer)
	begin_array  = _make_write_func(_ajson_write_begin_array)
	end_array    = _make_write_func(_ajson_write_end_array)
	begin_object = _make_write_func(_ajson_write_begin_object)
	end_object   = _make_write_func(_ajson_write_end_object)

	@_make_write_func
	def string(ptr, buffer, size, value):
		buf = value.encode('utf-8')
		return _ajson_write_string(ptr, buffer, size, buf, len(buf), ENC_UTF8)

	def value(self,obj):
		refs = set()

		def _write(obj):
			if obj is None:
				for data in self.null():
					yield data

			elif isinstance(obj, _Strings):
				for data in self.string(obj):
					yield data

			elif isinstance(obj, float):
				for data in self.number(obj):
					yield data

			elif isinstance(obj, _Ints):
				if obj < -0x8000000000000000 or obj > 0x7fffffffffffffff:
					for data in self.number(obj):
						yield data
				else:
					for data in self.integer(obj):
						yield data

			elif isinstance(obj, dict):
				ref = id(obj)
				if ref in refs:
					raise ValueError("cannot serialize recursive data structure")

				refs.add(ref)

				for data in self.begin_object():
					yield data

				for key in obj:
					for data in self.string(key):
						yield data

					for data in _write(obj[key]):
						yield data

				for data in self.end_object():
					yield data

				refs.remove(ref)

			elif isinstance(obj, _Lists) or hasattr(obj, '__iter__'):
				ref = id(obj)
				if ref in refs:
					raise ValueError("cannot serialize recursive data structure")

				refs.add(ref)

				for data in self.begin_array():
					yield data

				for item in obj:
					for data in _write(item):
						yield data

				for data in self.end_array():
					yield data

				refs.remove(ref)

			else:
				raise TypeError("object has unhandeled type: %r" % obj)

		return _write(obj)

def _make_file_write_func(write_func):
	@wraps(write_func)
	def _write_func(self,*args):
		fp     = self._file
		ptr    = self._writer
		buf    = self._buffer
		bufsiz = len(buf)
		size   = write_func(ptr, buf, bufsiz, *args)

		while size == bufsiz:
			fp.write(buf)
			size = _ajson_write_continue(ptr, buf, bufsiz)

		if size < 0:
			_error_from_errno()

		if size > 0:
			fp.write(memoryview(buf)[:size])

	return _write_func

class FileWriter(WriterBase):
	__slots__ = '_file',

	def __init__(self,file,flags=WRITER_FLAGS_NONE,indent=None,buffer_size=DEFAULT_BUFFER_SIZE):
		WriterBase.__init__(self,flags,indent,buffer_size)
		self._file = file

	null         = _make_file_write_func(_ajson_write_null)
	boolean      = _make_file_write_func(_ajson_write_boolean)
	number       = _make_file_write_func(_ajson_write_number)
	integer      = _make_file_write_func(_ajson_write_integer)
	begin_array  = _make_file_write_func(_ajson_write_begin_array)
	end_array    = _make_file_write_func(_ajson_write_end_array)
	begin_object = _make_file_write_func(_ajson_write_begin_object)
	end_object   = _make_file_write_func(_ajson_write_end_object)

	@_make_file_write_func
	def string(ptr, buffer, size, value):
		buf = value.encode('utf-8')
		return _ajson_write_string(ptr, buffer, size, buf, len(buf), ENC_UTF8)

	def value(self,obj):
		refs = set()

		def _write(obj):
			if obj is None:
				self.null()

			elif isinstance(obj, _Strings):
				self.string(obj)

			elif isinstance(obj, float):
				self.number(obj)

			elif isinstance(obj, _Ints):
				if obj < -0x8000000000000000 or obj > 0x7fffffffffffffff:
					self.number(obj)
				else:
					self.integer(obj)

			elif isinstance(obj, dict):
				ref = id(obj)
				if ref in refs:
					raise ValueError("cannot serialize recursive data structure")

				refs.add(ref)

				self.begin_object()

				for key in obj:
					self.string(key)
					_write(obj[key])

				self.end_object()

				refs.remove(ref)

			elif isinstance(obj, _Lists) or hasattr(obj, '__iter__'):
				ref = id(obj)
				if ref in refs:
					raise ValueError("cannot serialize recursive data structure")

				refs.add(ref)

				self.begin_array()

				for item in obj:
					_write(item)

				self.end_array()

				refs.remove(ref)

			else:
				raise TypeError("object has unhandeled type: %r" % obj)

		_write(obj)

def dump(obj, stream, ensure_ascii=True, indent=None, buffer_size=DEFAULT_BUFFER_SIZE):
	FileWriter(
		stream,
		WRITER_FALG_ASCII if ensure_ascii else WRITER_FLAGS_NONE,
		indent,buffer_size).value(obj)

def dumpb(obj, ensure_ascii=True, indent=None, buffer_size=DEFAULT_BUFFER_SIZE):
	return bytes().join(Writer(
		WRITER_FALG_ASCII if ensure_ascii else WRITER_FLAGS_NONE,
		indent,buffer_size).value(obj))

def dumps(obj, ensure_ascii=True, indent=None, buffer_size=DEFAULT_BUFFER_SIZE):
	return dumpb(obj, ensure_ascii, indent, buffer_size).decode('utf-8')

VERSION = (_ajson_version_major(), _ajson_version_minor(), _ajson_version_patch())
