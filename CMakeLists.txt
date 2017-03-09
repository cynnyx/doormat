cmake_minimum_required(VERSION 3.0)
project(doormat)

#
# Building in-tree is not allowed (we take care of your craziness).
#
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

#
# CMake project configuration
#
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

if( NOT $ENV{RELEASE_CANDIDATE} STREQUAL "" )
	set(DOORMAT_BUILD_MODE "RC")
	EXEC_PROGRAM(${CMAKE_SOURCE_DIR}/utils/revision.sh ARGS "--$ENV{RELEASE_CANDIDATE}"
		OUTPUT_VARIABLE PROJECT_VERSION)
else()
	set(DOORMAT_BUILD_MODE "${CMAKE_BUILD_TYPE}")
	EXEC_PROGRAM(${CMAKE_SOURCE_DIR}/utils/revision.sh ARGS -g
		OUTPUT_VARIABLE PROJECT_VERSION)
endif()

set(PROJECT_NAME "doormat")
string(TOLOWER "${PROJECT_VERSION}" PROJECT_VERSION)

set(PROJECT_AUTHOR "Cynny SpA")
set(PROJECT_YEAR_COPYRIGHT "2016")
set(PROJECT_AUTHOR_EMAIL "michele.caini@cynny.com" "tazio.ceri@cynny.com" "fabrizio.lasagni@cynny.com" "alessandro.lenzi@cynny.com" "paolo.monteverde@cynny.com")
set(SETTINGS_ORGANIZATION "Cynny SpA")
set(SETTINGS_APPLICATION "${PROJECT_NAME}")
set(PACKAGE_CHIEF "${PROJECT_AUTHOR} <${PROJECT_AUTHOR_EMAIL}>")

set(PROJECT_CMAKE_MODULES cmake/modules)
set(PROJECT_CONFIGURATION_DIRECTORY etc)
set(PROJECT_CONFIGURATION_OUTPUT_DIRECTORY etc/doormat)
set(PROJECT_RUNTIME_OUTPUT_DIRECTORY bin)
set(PROJECT_LIBRARY_OUTPUT_DIRECTORY lib)
set(PROJECT_ARCHIVE_OUTPUT_DIRECTORY lib/static)
set(PROJECT_SOURCES src)
set(PROJECT_UTILS utils)
set(PROJECT_TESTS test)
set(PROJECT_DEPS_DIR deps)
set(PROJECT_DOCS docs)
set(DOORMAT_COMMON_ARCHIVE_LIB doormat_common)
set(BASE_DIR "/opt/doormat")
set(CMAKE_MODULE_PATH
	"${CMAKE_MODULE_PATH}"
	"${CMAKE_SOURCE_DIR}/${PROJECT_CMAKE_MODULES}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
	"${PROJECT_BINARY_DIR}/${PROJECT_RUNTIME_OUTPUT_DIRECTORY}")

set(CMAKE_CONFIGURATION_OUTPUT_DIRECTORY
	"${PROJECT_BINARY_DIR}/${PROJECT_CONFIGURATION_OUTPUT_DIRECTORY}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
	"${PROJECT_BINARY_DIR}/${PROJECT_LIBRARY_OUTPUT_DIRECTORY}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
	"${PROJECT_BINARY_DIR}/${PROJECT_ARCHIVE_OUTPUT_DIRECTORY}")

message("-- Version ${PROJECT_VERSION}")
message("-- Build mode ${CMAKE_BUILD_TYPE}")

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
	include(CodeCoverage)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} --coverage -fprofile-arcs -ftest-coverage")
	message("-- Code Coverage taken")
endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)

#
# COMPILER stuff
#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -Wno-logical-op-parentheses")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++11 -DBOOST_LOG_DYN_LINK -Wall")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-logical-op-parentheses")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D'VERSION=\"${PROJECT_VERSION}\"'")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-error=parentheses -Wno-error=unused-function -Wno-error=sign-compare")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")

if(ENABLE_SAN)
	message("-- Address sanitizer enabled")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
endif(ENABLE_SAN)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

#
# Required packages
#
#if(NOT BOOST_ROOT)
#	set(BOOST_ROOT "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/boost")
#endif(NOT BOOST_ROOT)

if(NOT GMOCK_ROOT)
	set(GMOCK_ROOT "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/gtest/googlemock")
endif(NOT GMOCK_ROOT)

if(NOT OPENSSL_ROOT_DIR)
	set(OPENSSL_ROOT_DIR "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/openssl")
endif(NOT OPENSSL_ROOT_DIR)
set(OPENSSL_USE_STATIC_LIBS FALSE)
set(NGHTTP2_ROOT "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/nghttp2")
set(CITYHASH_ROOT "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/cityhash")
set(Boost_USE_STLPORT ON)
set(Boost_USE_MULTITHREADED ON)
set(SPDLOG_ROOT "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/spdlog")
set(UVW_ROOT_DIR "${PROJECT_SOURCE_DIR}/${PROJECT_DEPS_DIR}/uvw")
#context 
find_package(Boost 1.54 COMPONENTS system thread log program_options regex filesystem REQUIRED )
find_package(UVW REQUIRED)
find_package(OpenSSL 1.0.2 REQUIRED)
find_package(Threads REQUIRED)
find_package(NgHTTP2 REQUIRED)
find_package(Cynnypp REQUIRED)
find_package(CityHash REQUIRED)
find_package(GoogleTest REQUIRED)
find_package(GoogleMock REQUIRED)
find_package(ZLIB REQUIRED)
find_package(spdlog REQUIRED)

#
# Doxygen (documentation)
#

#DISABLED NOW BUT NEEDED SOON

# find_package(Doxygen 1.8)
#
# if(DOXYGEN_FOUND)
# 	set(DOXY_SOURCE_DIRECTORY ${PROJECT_SOURCES})
# 	set(DOXY_OUTPUT_DIRECTORY ${PROJECT_DOCS})
# 	set(DOXY_IN_FILE doxy.in)
# 	set(DOXY_CFG_FILE doxy.cfg)
# 	set(DOXY_TARGET documentation)
#
# 	configure_file(${DOXY_IN_FILE} ${DOXY_CFG_FILE})
#
# 	add_custom_target(
# 		${DOXY_TARGET} ALL
# 		COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/${DOXY_CFG_FILE}
# 		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
# 		VERBATIM
# 		SOURCES ${DOXY_IN_FILE}
# 	)
# endif(DOXYGEN_FOUND)

file(COPY resources/test/
	DESTINATION "${CMAKE_CONFIGURATION_OUTPUT_DIRECTORY}")

file(COPY resources/certificates
	DESTINATION "${CMAKE_CONFIGURATION_OUTPUT_DIRECTORY}")


file(COPY resources/pages
	DESTINATION "${CMAKE_CONFIGURATION_OUTPUT_DIRECTORY}")

# Is this needed??
#file(COPY resources/logs
#	DESTINATION "${CMAKE_CONFIGURATION_OUTPUT_DIRECTORY}/logs")
#

#
# INSTALLATION
#
set(DOORMAT_CONFIG_FILES
	"resources/doormat.config"
	"resources/certificates.config"
	"resources/error_files.config"
	"resources/ports.config"
	"resources/log.config"
	"resources/headers"
	"resources/keypass"
)

install(DIRECTORY "resources/pages"
	DESTINATION "${BASE_DIR}/share/"
	COMPONENT executable)

install(FILES ${DOORMAT_CONFIG_FILES}
	DESTINATION "${BASE_DIR}/etc/"
	PERMISSIONS OWNER_WRITE GROUP_WRITE WORLD_READ GROUP_READ OWNER_READ
	COMPONENT executable)

if(ENABLE_SAN)
	install(FILES "utils/doormat.init.asan"
		DESTINATION "${BASE_DIR}/bin/"
		RENAME "doormat.init"
		PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
		COMPONENT executable)
else()
	install(FILES "utils/doormat.init"
		DESTINATION "${BASE_DIR}/bin/"
		PERMISSIONS OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ OWNER_READ GROUP_EXECUTE OWNER_EXECUTE
		COMPONENT executable)
endif(ENABLE_SAN)

#
# CPACK stuff
#
set(CPACK_MONOLITHIC_INSTALL 0)
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_AUTHOR}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-${DOORMAT_BUILD_MODE}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Doormat reverse proxy")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${BASE_DIR}")
set(CPACK_PACKAGING_INSTALL_DIRECTORY "${BASE_DIR}")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEB_COMPONENT_INSTALL 1)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tazio Ceri <tazio.ceri@cynny.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "Network" )
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-log1.54.0, libboost-chrono1.54.0, libboost-context1.54.0, libboost-system1.54.0, libboost-regex1.54.0, libboost-filesystem1.54.0, libboost-thread1.54.0, libboost-program-options1.54.0, libboost-log1.54.0")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/utils/preinst;${CMAKE_CURRENT_SOURCE_DIR}/utils/postinst")

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(CPACK_DEBIAN_PACKAGE_DEBUG 1)
endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
include(CPack)

#
# Targets
#
add_definitions(
	-DPROJECT_NAME=${PROJECT_NAME}
	-DPROJECT_VERSION=${PROJECT_VERSION}
)
add_subdirectory(${PROJECT_SOURCES})
add_subdirectory(${PROJECT_TESTS})

#
#QT Creator stuff
#
file(
	GLOB_RECURSE
	PROJECT_FILES
		${PROJECT_SOURCE_DIR}/${PROJECT_CMAKE_MODULES}/*.cmake
		${PROJECT_SOURCE_DIR}/${PROJECT_SOURCES}/*.cpp
		${PROJECT_SOURCE_DIR}/${PROJECT_SOURCES}/*.hpp
		${PROJECT_SOURCE_DIR}/${PROJECT_SOURCES}/*.h
		${PROJECT_SOURCE_DIR}/*.in
)

add_custom_target(
	QTCREATOR_FALLBACK ALL
	COMMENT "Feel free to ignore this target, but please do not remove it."
	SOURCES ${PROJECT_FILES}
)
##End of QtCreator

include(CTest)
