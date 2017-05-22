#
# Doormat tests configuration
#

set(DOORMAT_TEST_EXECUTABLE doormattest)

set(DOORMAT_TESTCASES_SOURCES
	main.cpp
	base64_test.cpp
	codec_test.cpp
	configuration_parser_test.cpp
	cor_test.cpp
	sni_solver_test.cpp
	dstring_factory_test.cpp
	dstring_test.cpp
	error_test.cpp
	#handler_test.cpp
	header_conf_test.cpp
	inspector_serializer_test.cpp
	ipv4matcher_test.cpp
	log_test.cpp
	reusable_buffer_test.cpp
	stats_test.cpp
	testcommon.cpp
	utils_test.cpp
	mock_server/mock_server.cpp
	mocks/mock_connector.cpp
	network/communicator_test.cpp
	nodes/common.cpp
	cloudiapooltest.cpp
	endpoints/tree_test.cpp
	endpoints/chain_factory.cpp
	network/dns_factory_test.cpp
	nodes/client_wrapper_test.cpp
	http/server/server_connection_test.cpp
	http/client/client_connection_test.cpp
	http_client_test.cpp
)

add_executable(${DOORMAT_TEST_EXECUTABLE} ${DOORMAT_TESTCASES_SOURCES})

target_include_directories(
	${DOORMAT_TEST_EXECUTABLE}
	PRIVATE ${CMAKE_SOURCE_DIR}/${PROJECT_TESTS}
	PRIVATE ${NGHTTP2_INCLUDE_DIR}
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR}
	PRIVATE ${GOOGLETEST_INCLUDE_DIR}
	PRIVATE ${GMOCK_INCLUDE_DIRS}
	PRIVATE ${CYNNYPP_INCLUDE_DIRS}
	PRIVATE ${CITYHASH_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
	PRIVATE ${SPDLOG_INCLUDE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
	${DOORMAT_TEST_EXECUTABLE}
	PRIVATE ${DOORMAT_COMMON_ARCHIVE_LIB}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	PRIVATE ${GOOGLETEST_LIBRARY}
	PRIVATE ${GOOGLETEST_MAIN_LIBRARY}
	PRIVATE ${GMOCK_BOTH_LIBRARIES}
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE ${NGHTTP2_LIBRARY}
	PRIVATE ${NGHTTP2_ASIO_LIBRARY}
	PRIVATE ${OPENSSL_LIBRARIES}
	PRIVATE ${CYNNYPP_LIBRARIES}
	PRIVATE ${CITYHASH_LIBRARIES}
	PRIVATE ${ZLIB_LIBRARIES}
)
