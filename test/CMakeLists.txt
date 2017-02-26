#
# Doormat tests configuration
#

set(DOORMAT_TEST_EXECUTABLE doormattest)

set(DOORMAT_TESTCASES_SOURCES
	main.cpp
	board_map_test.cpp
	base64_test.cpp
	codec_test.cpp
	configuration_parser_test.cpp
	cor_test.cpp
	dstring_factory_test.cpp
	dstring_test.cpp
	error_test.cpp
	handler_test.cpp
	header_conf_test.cpp
	inspector_serializer_test.cpp
	ipv4matcher_test.cpp
	log_test.cpp
	reusable_buffer_test.cpp
	size_client_integration_test.cpp
	stats_test.cpp
	testcommon.cpp
	utils_test.cpp
	cache/cache_test.cpp
	cache/policy_test.cpp
	cache/translation_unit_test.cpp
	cache/ttl_cleaner_test.cpp
	mock_server/mock_server.cpp
	network/communicator_test.cpp
        network/socket_pool_test.cpp
	nodes/cache_cleaner_test.cpp
	nodes/cache_manager_test.cpp
	nodes/cache_request_normalizer_test.cpp
	nodes/client_wrapper_test.cpp
	nodes/common.cpp
	nodes/configurable_header_filter_test.cpp
	nodes/date_setter_test.cpp
	nodes/error_file_provider_test.cpp
	nodes/error_producer_test.cpp
	nodes/franco_host_test.cpp
	nodes/gzip_filter_test.cpp
	nodes/header_filter_test.cpp
	nodes/id_header_adder_test.cpp
	nodes/ip_filter_test.cpp
	nodes/method_filter_test.cpp
	nodes/request_stats_test.cpp
	nodes/size_filter_test.cpp
	nodes/sys_filter_test.cpp
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
