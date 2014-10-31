include_directories("${PROJECT_SOURCE_DIR}/src")

add_executable(tokens tokens.c)
target_link_libraries(tokens ${AJSON_LIB_NAME})

add_custom_target(tests
	"${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh"
	"${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS tokens)

add_custom_target(memcheck
	"${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh"
	--memcheck
	"${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS tokens)
