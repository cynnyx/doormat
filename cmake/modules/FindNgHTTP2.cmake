# FindNgHTTP2
# ---------
#
# Locate NgHTTP2
#
# This module defines:
#
# ::
#
#   NGHTTP2_LIBRARY, the name of the library to link against
#	NGHTTP2_ASIO_LIBRARY
#   NGHTTP2_INCLUDE_DIRS, where to find the headers
#   NGHTTP2_FOUND, if false, do not try to link against
#

find_path(NGHTTP2_INCLUDE_DIR NAMES nghttp2/nghttp2.h
	HINTS ${NGHTTP2_ROOT}/INSTALL/include
)

find_library(NGHTTP2_LIBRARY NAMES libnghttp2.so
	HINTS ${NGHTTP2_ROOT}/INSTALL/lib
)

find_library(NGHTTP2_ASIO_LIBRARY NAMES libnghttp2_asio.so
	HINTS ${NGHTTP2_ROOT}/INSTALL/lib
)

set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
	NGHTTP2
	FOUND_VAR NGHTTP2_FOUND
	REQUIRED_VARS NGHTTP2_INCLUDE_DIRS NGHTTP2_LIBRARIES
)

mark_as_advanced(NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIR)
