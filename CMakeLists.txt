cmake_minimum_required(VERSION 2.8.8)

project(ajson)

include(CheckCSourceCompiles)

check_c_source_compiles("
int main() {
	void *ptr = &&label;
	goto *ptr;
label:
	return 0;
}
" HAVE_ADDRESS_FROM_LABEL)

check_c_source_compiles("#include <stdio.h>
int main() { char buf[64]; return snprintf(buf, sizeof(buf), \"test\"); }" HAVE_SNPRINTF)

option(AJSON_USE_GNUC_ADDRESS_FROM_LABEL
	"Use GNUC extension to use label addresses for improved performance"
	${HAVE_ADDRESS_FROM_LABEL})
option(BUILD_SHARED_LIBS "Build as shared library" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_PERF "Build performance tests" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_DOCS "Build documentation" OFF)

set(AJSON_MAJOR_VERSION 1)
set(AJSON_MINOR_VERSION 0)
set(AJSON_PATCH_VERSION 0)

set(AJSON_NAME ajson)
set(AJSON_VERSION ${AJSON_MAJOR_VERSION}.${AJSON_MINOR_VERSION}.${AJSON_PATCH_VERSION})

if(BUILD_SHARED_LIBS)
	set(AJSON_LIB_NAME ${AJSON_NAME})
else()
	set(AJSON_LIB_NAME ${AJSON_NAME}_static)
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
	set(CMAKE_INSTALL_LIBDIR "lib")
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -std=gnu99 -pedantic -fvisibility=hidden")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3")
	set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS} -O3 -g")
endif()

if(NOT WIN32)
	find_library(M_LIBRARY m)
	if(M_LIBRARY)
		set(AJSON_PRIVATE_LIBS -l${M_LIBRARY})
	else()
		message(STATUS "math library 'libm' not found")
	endif()
else()
	# not needed on windows
	set(M_LIBRARY "")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ajson.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/${AJSON_NAME}.pc @ONLY)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/${AJSON_NAME}.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# for config.h
include_directories("${PROJECT_BINARY_DIR}/src")

add_subdirectory(src)

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(BUILD_PERF)
	add_subdirectory(perf)
endif()

if(BUILD_TESTS)
	add_subdirectory(test)
endif()

if(BUILD_DOCS)
	add_subdirectory(doc)
endif()

# uninstall target
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
