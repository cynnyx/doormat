# app running a dummy server
add_executable(app main.cpp)

target_include_directories(
        app
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        PRIVATE ${Boost_INCLUDE_DIRS}
)

target_link_libraries(
        app
        PRIVATE ${CMAKE_THREAD_LIBS_INIT}
        PRIVATE ${Boost_LIBRARIES}
        PRIVATE ${OPENSSL_LIBRARIES}
        PRIVATE ${NGHTTP2_LIB}
        PRIVATE ${NGHTTP2_ASIO_LIB}
        PRIVATE ${CITYHASH_LIB}
        PRIVATE ${ZLIB_LIBRARIES}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../build/lib/libdoormat_shared.so"
)
