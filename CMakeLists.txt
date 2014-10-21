cmake_minimum_required(VERSION 2.8.8)

project(ajson)

option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
#option(BUILD_DOCS "Build doxygen documentation" OFF)
option(BUILD_TESTS "Build tests" OFF)

set(AJSON_MAJOR_VERSION 1)
set(AJSON_MINOR_VERSION 0)
set(AJSON_PATCH_VERSION 0)

set(AJSON_NAME ajson${AJSON_MAJOR_VERSION}${AJSON_MINOR_VERSION})
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
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -std=gnu99")

	if(NOT(CMAKE_BUILD_TYPE STREQUAL "Debug"))
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
	endif()

	if(NOT(CMAKE_COMPILER_IS_MINGW64))
		# can't be pedantic for mingw64 because of format strings
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
	endif()
endif()

#if(NOT WIN32)
#	find_library(M_LIBRARY
#		NAMES m
#		PATHS /usr/lib /usr/local/lib)
#	if(M_LIBRARY)
#		set(AJSON_PRIVATE_LIBS -l${M_LIBRARY})
#	else()
#		message(STATUS "math library 'libm' not found")
#	endif()
#else()
#	# not needed on windows
#	set(M_LIBRARY "")
#endif()

# from libpng
# Set a variable with CMake code which:
# Creates a symlink from src to dest (if possible) or alternatively
# copies if different.
macro(ajson_generate_symlink_code CODE SRC DEST)
	if(WIN32 AND NOT CYGWIN)
		set(_ajson_gsc_message "Copying ${SRC} to ${DEST} if needed")
		set(_ajson_gsc_operation "copy_if_different")
	else()
		set(_ajson_gsc_message "Symlinking ${SRC} to ${DEST}")
		set(_ajson_gsc_operation "create_symlink")
	endif()

	set(${CODE} "
		message(STATUS \"${_ajson_gsc_message}\")
		execute_process(COMMAND \${CMAKE_COMMAND} -E ${_ajson_gsc_operation}
			\"${SRC}\" \"${DEST}\")
		")
endmacro()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ajson.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/${AJSON_NAME}.pc @ONLY)

ajson_generate_symlink_code(AJSON_PC_INSTALL_CODE
  ${CMAKE_CURRENT_BINARY_DIR}/${AJSON_NAME}.pc
  ${CMAKE_CURRENT_BINARY_DIR}/ajson.pc)
install(CODE ${AJSON_PC_INSTALL_CODE})
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/ajson.pc
	${CMAKE_CURRENT_BINARY_DIR}/${AJSON_NAME}.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# for config.h
include_directories("${PROJECT_BINARY_DIR}/src")

add_subdirectory(src)

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(BUILD_TESTS)
	include(CTest)

	enable_testing()

	set(CTEST_MEMORYCHECK_COMMAND "valgrind")
	set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "-v --leak-check=full")

	add_subdirectory(test)
endif()

#if(BUILD_DOCS)
#	find_package(Doxygen)
#	if(NOT DOXYGEN_FOUND)
#		message(FATAL_ERROR
#			"Doxygen is needed to build the documentation.")
#	endif()
#
#	configure_file(Doxyfile.in 
#		"${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
#
#	add_custom_target(docs ALL
#		COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
#		SOURCES ${PROJECT_BINARY_DIR}/Doxyfile)
#endif()

# uninstall target
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
