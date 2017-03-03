# Findspdlog
# ---------
#
#
# This module defines:
#
# ::
#
#   SPDLOG_INCLUDE_DIRS, where to find the headers
#   SPDLOG_FOUND, if false, do not try to link against
#


# ------------- find paths where the stuff is supposed to be

# find the include dir, then we'll know where all the other stuff is
find_path(SPDLOG_INCLUDE_DIR NAMES spdlog/spdlog.h
		HINTS ${CMAKE_SOURCE_DIR}/deps/spdlog/include
		)

if(NOT SPDLOG_INCLUDE_DIR)
	message(SEND_ERROR "Could not find SPDLOG_INCLUDE_DIR_RAW")
	return()
endif()

message("found spdlog include dir: ${SPDLOG_INCLUDE_DIR}")

# ------------- export variables

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
		SPDLOG
		FOUND_VAR SPDLOG_FOUND
		REQUIRED_VARS SPDLOG_INCLUDE_DIR
)

# clear all unuseful variables
mark_as_advanced(
		SPDLOG_INCLUDE_DIR
)
