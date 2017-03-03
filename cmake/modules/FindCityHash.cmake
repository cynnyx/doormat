# FindCityHash
# ---------
#
# Locate CityHash
#
# This module defines:
#
# ::
#
#   CITYHASH_LIBRARY, the name of the library to link against
#   CITYHASH_INCLUDE_DIRS, where to find the headers
#   CITYHASH_FOUND, if false, do not try to link against
#

find_path(CITYHASH_INCLUDE_DIR city.h
	HINTS "${CITYHASH_ROOT}/build/include"
)

find_library(CITYHASH_LIBRARY libcityhash.so
	HINTS "${CITYHASH_ROOT}/build/lib"
)

set(CITYHASH_INCLUDE_DIRS ${CITYHASH_INCLUDE_DIR})
set(CITYHASH_LIBRARIES ${CITYHASH_LIBRARY})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
	CITYHASH
	FOUND_VAR CITYHASH_FOUND
	REQUIRED_VARS CITYHASH_INCLUDE_DIRS CITYHASH_LIBRARIES
)

mark_as_advanced(CITYHASH_LIBRARY CITYHASH_INCLUDE_DIR)
