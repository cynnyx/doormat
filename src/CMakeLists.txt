#
# Doormat executable configuration
#

set(DOORMAT_EXECUTABLE doormat)
set(NGHTTP2_LIB "nghttp2")
set(NGHTTP2_ASIO_LIB "nghttp2_asio")
set(CYNNYPP_LIB "cynnypp")
set(CITYHASH_LIB "cityhash")
set(SPDLOG_LIB "spdlog")

# common sources: will be packed inside a static lib and linked against all executables
set(DOORMAT_COMMON_SOURCES
	board/board.cpp
	board/board_map.cpp
	configuration/configuration_wrapper.cpp
	configuration/header_configuration.cpp
	constants.cpp
	daemon.cpp
	init.cpp
	errors/error_factory_async.cpp
	errors/error_messages.cpp
	http/http_codec.cpp
	http/http_commons.cpp
	http/http_structured_data.cpp
	http/http_response.cpp
	http/http_request.cpp
	http_parser/http_parser.c
	http/peer.cpp
	http_server.cpp
	io_service_pool.cpp
	IPV4Network.cpp
	log/format.cpp
	log/log.cpp
	log/access_record.cpp
	protocol/handler_factory.cpp
	protocol/handler_http1.cpp
	protocol/handler_http2.cpp
	protocol/stream_handler.cpp
	requests_manager/client_wrapper.cpp
	requests_manager/header_filter.cpp
	requests_manager/interreg_filter.cpp
	requests_manager/error_file_provider.cpp
	requests_manager/error_producer.cpp
	requests_manager/method_filter.cpp
	requests_manager/request_stats.cpp
	requests_manager/sys_filter.cpp
	requests_manager/id_header_adder.cpp
	requests_manager/franco_host.cpp
	requests_manager/configurable_header_filter.cpp
	requests_manager/cache_manager/cache_manager.cpp
	requests_manager/gzip_filter.cpp
	stats/stats_manager.cpp
	service_locator/service_locator.cpp
	utils/resource_reader.cpp
	utils/dstring.cpp
	utils/dstring_factory.cpp
	utils/sni_solver.cpp
	utils/utils.cpp
	utils/base64.cpp
	utils/log_wrapper.cpp
	errors/error_message_details.cpp
	errors/internal_error.cpp
	requests_manager/size_filter.cpp
	fs/fs_manager_wrapper.cpp
	http2/session.cpp
	http2/stream.cpp
	http2/http2alloc.cpp
	configuration/configuration_maker.cpp
	configuration/cache_normalization_rule.cpp
	network/socket_pool.cpp
	network/magnet.cpp
	log/inspector_serializer.cpp
	requests_manager/cache_cleaner.cpp
)

#
# ----------------- doormat_common archive library -------------------
#
add_library(${DOORMAT_COMMON_ARCHIVE_LIB} STATIC ${DOORMAT_COMMON_SOURCES})

target_include_directories(
	${DOORMAT_COMMON_ARCHIVE_LIB}
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR}
	PRIVATE ${NGHTTP2_INCLUDE_DIR}
	PRIVATE ${CYNNYPP_INCLUDE_DIRS}
	PRIVATE ${CITYHASH_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
	PRIVATE ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(
	${DOORMAT_COMMON_ARCHIVE_LIB}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE ${OPENSSL_LIBRARIES}
	PRIVATE ${NGHTTP2_LIB}
	PRIVATE ${NGHTTP2_ASIO_LIB}
	PRIVATE ${CYNNYPP_LIB}
	PRIVATE ${CITYHASH_LIB}
	PRIVATE ${ZLIB_LIBRARIES}
)

#
# ---------------------- doormat executable ------------------------
#

add_executable(${DOORMAT_EXECUTABLE} main.cpp)

add_library(${NGHTTP2_LIB} SHARED IMPORTED)
add_library(${NGHTTP2_ASIO_LIB} SHARED IMPORTED)
add_library(${CYNNYPP_LIB} SHARED IMPORTED)
add_library(${CITYHASH_LIB} SHARED IMPORTED)

set_property(TARGET ${NGHTTP2_LIB} PROPERTY IMPORTED_LOCATION ${NGHTTP2_LIBRARY})
set_property(TARGET ${NGHTTP2_ASIO_LIB} PROPERTY IMPORTED_LOCATION ${NGHTTP2_ASIO_LIBRARY})
set_property(TARGET ${CYNNYPP_LIB} PROPERTY IMPORTED_LOCATION ${CYNNYPP_LIBRARIES})
set_property(TARGET ${CITYHASH_LIB} PROPERTY IMPORTED_LOCATION ${CITYHASH_LIBRARIES})

target_include_directories(
	${DOORMAT_EXECUTABLE}
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR}
	PRIVATE ${NGHTTP2_INCLUDE_DIR}
	PRIVATE ${CYNNYPP_INCLUDE_DIRS}
	PRIVATE ${CITYHASH_INCLUDE_DIRS}
	PRIVATE ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(
	${DOORMAT_EXECUTABLE}
	PRIVATE ${DOORMAT_COMMON_ARCHIVE_LIB}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE ${OPENSSL_LIBRARIES}
	PRIVATE ${NGHTTP2_LIB}
	PRIVATE ${NGHTTP2_ASIO_LIB}
	PRIVATE ${CYNNYPP_LIB}
	PRIVATE ${CITYHASH_LIB}
)


#
# Installation
#
file(GLOB OPENSSLLIBS "${OPENSSL_ROOT_DIR}/lib*.so*")
foreach(LIB ${OPENSSLLIBS})
	install(FILES "${LIB}"
		DESTINATION "${BASE_DIR}/lib"
		PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
		COMPONENT executable)
endforeach(LIB)

install(FILES $<TARGET_SONAME_FILE:${NGHTTP2_LIB}>
	DESTINATION "${BASE_DIR}/lib"
	PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
	COMPONENT executable)

install(FILES $<TARGET_SONAME_FILE:${NGHTTP2_ASIO_LIB}>
	DESTINATION "${BASE_DIR}/lib"
	PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
	COMPONENT executable)

install(FILES $<TARGET_SONAME_FILE:${CYNNYPP_LIB}>
	DESTINATION "${BASE_DIR}/lib"
	PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
	COMPONENT executable)

install(FILES $<TARGET_SONAME_FILE:${CITYHASH_LIB}>
	DESTINATION "${BASE_DIR}/lib"
	PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
	COMPONENT executable)

install(TARGETS ${DOORMAT_EXECUTABLE}
	RUNTIME DESTINATION "${BASE_DIR}/bin"
	COMPONENT executable)
