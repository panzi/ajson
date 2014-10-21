include_directories("${PROJECT_SOURCE_DIR}/src")

set(AJSON_EXAMPLES prettyprint tokens)

foreach(example ${AJSON_EXAMPLES})
	add_executable(${example} ${example}.c)
	target_link_libraries(${example} ${AJSON_LIB_NAME})
endforeach()
