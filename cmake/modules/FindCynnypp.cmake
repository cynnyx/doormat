# FindCynnypp
# ---------
#
# Locate Cynnypp: the work on this file is ongoing; as far as now, it only search for shared libraries...
#
# This module defines:
#
# ::
#
#   CYNNYPP_LIBRARIES, the libraries to link against
#   CYNNYPP_INCLUDE_DIRS, where to find the headers
#   CYNNYPP_FOUND, if false, do not try to link against
#


# ------------- find paths where the stuff is supposed to be

# find the include dir, then we'll know where all the other stuff is
find_path(CYNNYPP_INCLUDE_DIR NAMES cynnypp/async_fs.hpp
	HINTS ${CMAKE_SOURCE_DIR}/deps/cynnypp/include
)

if(NOT CYNNYPP_INCLUDE_DIR)
	message(SEND_ERROR "Could not find CYNNYPP_INCLUDE_DIR")
	return()
endif()

# now we can set the src and lib dirs
set(CYNNYPP_SRC_DIR ${CYNNYPP_INCLUDE_DIR}/../src)
set(CYNNYPP_LIB_DIR ${CYNNYPP_INCLUDE_DIR}/../build/lib)



# ------------ search for the libs that actually are inside the lib dir

find_library(CYNNYPP_ASYNC_FS_LIBRARY NAMES libcynpp_async_fs.so
	HINTS ${CYNNYPP_LIB_DIR}
)

find_library(CYNNYPP_SWAP_BUF_LIBRARY NAMES libcynpp_swap_buf.so
	HINTS ${CYNNYPP_LIB_DIR}
)


# ------------- set the variables to be _exported_

set(CYNNYPP_INCLUDE_DIRS ${CYNNYPP_INCLUDE_DIR} ${CYNNYPP_SRC_DIR})

if(CYNNYPP_ASYNC_FS_LIBRARY)
	set(CYNNYPP_LIBRARIES ${CYNNYPP_ASYNC_FS_LIBRARY})
endif()
if(CYNNYPP_SWAP_BUF_LIBRARY)
	set(CYNNYPP_LIBRARIES ${CYNNYPP_SWAP_BUF_LIBRARY})
endif()


# ------------- export variables

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
	CYNNYPP
	FOUND_VAR CYNNYPP_FOUND
	REQUIRED_VARS CYNNYPP_INCLUDE_DIRS CYNNYPP_LIBRARIES
)

# clear all unuseful variables
mark_as_advanced(
	CYNNYPP_INCLUDE_DIR
	CYNNYPP_SRC_DIR
	CYNNYPP_LIB_DIR
	CYNNYPP_ASYNC_FS_LIBRARY
	CYNNYPP_SWAP_BUF_LIBRARY
)
