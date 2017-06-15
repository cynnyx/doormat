# app running a dummy server
add_executable(app main.cpp)

target_include_directories(
        app
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        PRIVATE ${Boost_INCLUDE_DIRS}
        PRIVATE ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(
        app
        PRIVATE ${DOORMAT_COMMON_SHARED_LIB}
        PRIVATE ${CMAKE_THREAD_LIBS_INIT}
        PRIVATE ${Boost_LIBRARIES}
        PRIVATE ${OPENSSL_LIBRARIES}
        PRIVATE ${NGHTTP2_LIB}
        PRIVATE ${NGHTTP2_ASIO_LIB}
        PRIVATE ${CITYHASH_LIB}
        PRIVATE ${ZLIB_LIBRARIES}
)
