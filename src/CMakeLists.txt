include(GenerateExportHeader)

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)

set(AJSON_SRCS parser.c callback_parser.c writer.c ajson.c ajson.h config.h.in)

add_compiler_export_flags()
add_library(${AJSON_LIB_NAME} ${AJSON_SRCS})
generate_export_header(${AJSON_LIB_NAME}
	EXPORT_MACRO_NAME AJSON_EXPORT
	EXPORT_FILE_NAME export.h
	STATIC_DEFINE AJSON_STATIC_LIB)

target_link_libraries(${AJSON_LIB_NAME} ${M_LIBRARY})

install(TARGETS ${AJSON_LIB_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
	 ajson.h
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	"${CMAKE_CURRENT_BINARY_DIR}/export.h"
	DESTINATION "include/${AJSON_NAME}")
