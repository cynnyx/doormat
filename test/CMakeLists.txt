#
# Doormat tests configuration
#

set(DOORMAT_TEST_EXECUTABLE doormattest)

set(DOORMAT_TESTCASES_SOURCES
	main.cpp
	base64_test.cpp
	codec_test.cpp
	sni_solver_test.cpp
	dstring_factory_test.cpp
	dstring_test.cpp
	error_test.cpp
	reusable_buffer_test.cpp
	testcommon.cpp
	utils_test.cpp
	mocks/mock_server/mock_server.cpp
	mocks/mock_connector/mock_connector.cpp
	network/dns_factory_test.cpp
	http/server/server_connection_test.cpp
	http/server/http2_test.cpp
	http/client/client_connection_test.cpp
	http_client_test.cpp
	connector_test.cpp 
	mocks/mock_handler/mock_handler.cpp
	mocks/mock_handler/mock_handler.h
	network/multiplexer_test.cpp)

add_executable(${DOORMAT_TEST_EXECUTABLE} ${DOORMAT_TESTCASES_SOURCES})

target_include_directories(
	${DOORMAT_TEST_EXECUTABLE}
	PRIVATE ${CMAKE_SOURCE_DIR}/${PROJECT_TESTS}
	PRIVATE ${NGHTTP2_INCLUDE_DIR}
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR}
	PRIVATE ${GOOGLETEST_INCLUDE_DIR}
	PRIVATE ${GMOCK_INCLUDE_DIRS}
	PRIVATE ${CITYHASH_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
	PRIVATE ${SPDLOG_INCLUDE_DIR}
	PRIVATE ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
	${DOORMAT_TEST_EXECUTABLE}
        PRIVATE ${DOORMAT_COMMON_SHARED_LIB}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	PRIVATE ${GOOGLETEST_LIBRARY}
	PRIVATE ${GOOGLETEST_MAIN_LIBRARY}
	PRIVATE ${GMOCK_BOTH_LIBRARIES}
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE ${NGHTTP2_LIBRARY}
	PRIVATE ${NGHTTP2_ASIO_LIBRARY}
	PRIVATE ${OPENSSL_LIBRARIES}
	PRIVATE ${CITYHASH_LIBRARIES}
	PRIVATE ${ZLIB_LIBRARIES}
)
