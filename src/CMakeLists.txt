#
# Doormat executable configuration
#

set(NGHTTP2_LIB "nghttp2")
set(NGHTTP2_ASIO_LIB "nghttp2_asio")
set(CYNNYPP_LIB "cynnypp")
set(CITYHASH_LIB "cityhash")
set(SPDLOG_LIB "spdlog")

# common sources: will be packed inside a static lib and linked against all executables
set(DOORMAT_COMMON_SOURCES
        http/http_codec.cpp
	http/http_commons.cpp
	http/http_structured_data.cpp
	http/http_response.cpp
	http/http_request.cpp
	http_parser/http_parser.c
	http_server.cpp
	http_client.cpp
	protocol/handler_factory.cpp
        utils/dstring.cpp
	utils/dstring_factory.cpp
	utils/sni_solver.cpp
	utils/utils.cpp
	utils/base64.cpp
	utils/log_wrapper.cpp
	errors/internal_error.cpp
	http2/session.cpp
        http2/stream.cpp
        http2/session_client.cpp
        http2/stream_client.cpp
        http2/http2alloc.cpp
        network/communicator/dns_communicator_factory.cpp
	http/connection.cpp
	http/server/server_connection.cpp
	http/server/request.cpp
	http/server/response.cpp
	http/client/client_connection.cpp
	http/client/request.cpp
	http/client/response.cpp
	protocol/http_handler.cpp
)


add_library(${DOORMAT_COMMON_SHARED_LIB} SHARED ${DOORMAT_COMMON_SOURCES})

target_include_directories(
	${DOORMAT_COMMON_SHARED_LIB}
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR}
	PRIVATE ${NGHTTP2_INCLUDE_DIR}
	PRIVATE ${CITYHASH_INCLUDE_DIRS}
	PRIVATE ${ZLIB_INCLUDE_DIRS}
	PRIVATE ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(
	${DOORMAT_COMMON_SHARED_LIB}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE ${OPENSSL_LIBRARIES}
	PRIVATE ${NGHTTP2_LIB}
	PRIVATE ${NGHTTP2_ASIO_LIB}
	PRIVATE ${CITYHASH_LIB}
	PRIVATE ${ZLIB_LIBRARIES}
)


#
# Installation
#

add_library(${NGHTTP2_LIB} SHARED IMPORTED)
add_library(${NGHTTP2_ASIO_LIB} SHARED IMPORTED)
add_library(${CYNNYPP_LIB} SHARED IMPORTED)
add_library(${CITYHASH_LIB} SHARED IMPORTED)
set_property(TARGET ${NGHTTP2_LIB} PROPERTY IMPORTED_LOCATION ${NGHTTP2_LIBRARY})
set_property(TARGET ${NGHTTP2_ASIO_LIB} PROPERTY IMPORTED_LOCATION ${NGHTTP2_ASIO_LIBRARY})
set_property(TARGET ${CYNNYPP_LIB} PROPERTY IMPORTED_LOCATION ${CYNNYPP_LIBRARIES})
set_property(TARGET ${CITYHASH_LIB} PROPERTY IMPORTED_LOCATION ${CITYHASH_LIBRARIES})

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
