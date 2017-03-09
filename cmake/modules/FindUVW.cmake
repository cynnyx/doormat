# FindUVW
# ---------
#
#
# This module defines:
#
# ::
#
#   UVW_INCLUDE_DIRS, where to find the headers
#   UVW_FOUND, if false, do not try to link against
#


# ------------- find paths where the stuff is supposed to be
# find the include dir, then we'll know where all the other stuff is
find_path(UVW_INCLUDE_DIR NAMES uvw.hpp
        HINTS ${UVW_ROOT_DIR}/src
        )

if(NOT UVW_INCLUDE_DIR)
    message(SEND_ERROR "Could not find UVW_INCLUDE_DIR")
    return()
endif()

message("-- found uvw include dir: ${UVW_INCLUDE_DIR}")

# ------------- export variables

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
        UVW
        FOUND_VAR UVW_FOUND
        REQUIRED_VARS UVW_INCLUDE_DIR
)

# clear all unuseful variables
mark_as_advanced(
        UVW_INCLUDE_DIR
)
