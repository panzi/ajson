#!/usr/bin/python

import ctypes
import errno

from functools import wraps
from ctypes    import get_errno
from errno     import EINVAL, ENOMEM
from os        import strerror
from io        import DEFAULT_BUFFER_SIZE

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

FLAG_INTEGER           = 1
FLAG_NUMBER_COMPONENTS = 2

FLAGS_NONE = 0
FLAGS_ALL  = (FLAG_INTEGER | FLAG_NUMBER_COMPONENTS)

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
	if err == EINVAL:
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

	def __init__(self,flags=FLAGS_NONE):
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

	__next__ = next

	def __iter__(self):
		return self

def parse_string(s,flags=FLAGS_NONE):
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

class _Parser(ctypes.Structure):
	pass
	
class _Writer(ctypes.Structure):
	pass

_ParserPtr = ctypes.POINTER(_Parser)
_WriterPtr = ctypes.POINTER(_Writer)

# load C functions from shared object
_lib = ctypes.CDLL("libajson10.so",use_errno=True)

_ajson_alloc = _lib.ajson_alloc
_ajson_alloc.argtypes = [ctypes.c_int]
_ajson_alloc.restype  = _ParserPtr

_ajson_free = _lib.ajson_free
_ajson_free.argtypes = [_ParserPtr]
_ajson_free.restype  = None

_ajson_feed = _lib.ajson_feed
_ajson_feed.argtypes = [_ParserPtr, ctypes.c_void_p, ctypes.c_size_t]
_ajson_feed.restype  = ctypes.c_int

_ajson_next_token = _lib.ajson_next_token
_ajson_feed.argtypes = [_ParserPtr]
_ajson_feed.restype  = ctypes.c_int

_ajson_get_flags = _lib.ajson_get_flags
_ajson_get_flags.argtypes = [_ParserPtr]
_ajson_get_flags.restype  = ctypes.c_int

_ajson_get_lineno = _lib.ajson_get_lineno
_ajson_get_lineno.argtypes = [_ParserPtr]
_ajson_get_lineno.restype  = ctypes.c_size_t

_ajson_get_columnno = _lib.ajson_get_columnno
_ajson_get_columnno.argtypes = [_ParserPtr]
_ajson_get_columnno.restype  = ctypes.c_size_t

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
_ajson_get_string.restype  = ctypes.c_char_p

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
_ajson_writer_alloc.argtypes = [ctypes.c_char_p]
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
_ajson_write_string.argtypes = [_WriterPtr, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p]
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

def _make_write_func(write_func):
	@wraps(write_func)
	def _write_func(self,*args):
		ptr    = self._writer
		buf    = self._buffer
		bufsiz = len(buf)
		size   = write_func(ptr, buf, bufsiz, *args)
		if size < 0:
			_error_from_errno()

		while size == bufsiz:
			yield buf.raw

			size = _ajson_write_continue(ptr, buf, bufsiz)
			
			if size < 0:
				_error_from_errno()

		if size > 0:
			yield buf[:size]

	return _write_func

class Writer(object):
	__slots__ = '_writer', '_buffer'

	def __init__(self,indent=None,buffer_size=DEFAULT_BUFFER_SIZE):
		if buffer_size < 1:
			raise ValueError("size musst be bigger than zero")
		self._buffer = ctypes.create_string_buffer(buffer_size)
		self._writer = _ajson_writer_alloc(None if indent is None else indent.encode('utf-8'))
		if not self._writer:
			_error_from_errno()

	@property
	def buffer_size(self):
		return len(self._buffer)

	write_null         = _make_write_func(_ajson_write_null)
	write_boolean      = _make_write_func(_ajson_write_boolean)
	write_number       = _make_write_func(_ajson_write_number)
	write_integer      = _make_write_func(_ajson_write_integer)
	write_begin_array  = _make_write_func(_ajson_write_begin_array)
	write_end_array    = _make_write_func(_ajson_write_end_array)
	write_begin_object = _make_write_func(_ajson_write_begin_object)
	write_end_object   = _make_write_func(_ajson_write_end_object)

	@_make_write_func
	def write_string(ptr, buffer, size, value):
		return _ajson_write_string(ptr, buffer, size, value.encode('utf-8'))

	def write(self,obj):
		refs = set()

		def _write(obj):
			if obj is None:
				for data in self.write_null():
					yield data

			elif isinstance(obj, _Strings):
				for data in self.write_string(obj):
					yield data

			elif isinstance(obj, float):
				for data in self.write_number(obj):
					yield data

			elif isinstance(obj, _Ints):
				if obj < -0x8000000000000000 or obj > 0x7fffffffffffffff:
					for data in self.write_number(obj):
						yield data
				else:
					for data in self.write_integer(obj):
						yield data

			elif isinstance(obj, dict):
				ref = id(obj)
				if ref in refs:
					raise ValueError("cannot serialize recursive data structure")

				refs.add(ref)

				for data in self.write_begin_object():
					yield data

				for key in obj:
					for data in self.write_string(key):
						yield data

					for data in _write(obj[key]):
						yield data

				for data in self.write_end_object():
					yield data

				refs.remove(ref)

			elif isinstance(obj, _Lists) or hasattr(obj, '__iter__'):
				ref = id(obj)
				if ref in refs:
					raise ValueError("cannot serialize recursive data structure")

				refs.add(ref)

				for data in self.write_begin_array():
					yield data

				for item in obj:
					for data in _write(item):
						yield data

				for data in self.write_end_array():
					yield data

				refs.remove(ref)

			else:
				raise TypeError("object has unhandeled type: %r" % obj)

		return _write(obj)

def dump(obj, stream, indent=None, buffer_size=DEFAULT_BUFFER_SIZE):
	for data in Writer(indent,buffer_size).write(obj):
		stream.write(data)

def dumpb(obj, indent=None, buffer_size=DEFAULT_BUFFER_SIZE):
	return bytes().join(Writer(indent,buffer_size).write(obj))

def dumps(obj, indent=None, buffer_size=DEFAULT_BUFFER_SIZE):
	return dumpb(obj, indent, buffer_size).decode('utf-8')
