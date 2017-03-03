# FindGoogleTest
# ---------
#
# Locate Google Test Framework
#
# This module defines:
#
# ::
#
#   GOOGLETEST_INCLUDE_DIRS, where to find the headers
#   GOOGLETEST_LIBRARIES, the libraries against which to link
#   GOOGLETEST_FOUND, if false, do not try to use the above mentioned vars
#
# Accepts the following variables as input:
#
#   GTEST_ROOT - (as a CMake or environment variable)
#                The root directory of the gtest module
#
#   GTEST_BUILD - (as a CMake or environment variable)
#                The directory where gtest library are
#
#-----------------------

if(NOT GTEST_ROOT)
	set(GTEST_ROOT "${CMAKE_SOURCE_DIR}/${PROJECT_DEPS_DIR}/gtest")
endif(NOT GTEST_ROOT)

if(NOT GTEST_BUILD)
	set(GTEST_BUILD "${GTEST_ROOT}/googletest")
endif(NOT GTEST_BUILD)

find_path(
	GOOGLETEST_INCLUDE_DIR NAMES gtest/gtest.h
	PATHS ${GTEST_ROOT}/googletest/include/
	NO_DEFAULT_PATH
)

find_library(
	GOOGLETEST_LIBRARY NAMES gtest
	PATHS ${GTEST_BUILD}
	NO_DEFAULT_PATH
)

find_library(
	GOOGLETEST_MAIN_LIBRARY NAMES gtest_main
	PATHS ${GTEST_BUILD}
	NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
	GOOGLETEST
	FOUND_VAR GOOGLETEST_FOUND
	REQUIRED_VARS
		GOOGLETEST_LIBRARY
		GOOGLETEST_MAIN_LIBRARY
		GOOGLETEST_INCLUDE_DIR
)

if(GOOGLETEST_FOUND)
	set(
		GOOGLETEST_LIBRARIES
		${GOOGLETEST_LIBRARY}
		${GOOGLETEST_MAIN_LIBRARY}
	)

	set(GOOGLETEST_INCLUDE_DIRS
		${GOOGLETEST_INCLUDE_DIR}
		${GOOGLEMCK_INCLUDE_DIR}
	)
endif(GOOGLETEST_FOUND)


mark_as_advanced(
	GOOGLETEST_INCLUDE_DIR
	GOOGLETEST_LIBRARY
	GOOGLETEST_MAIN_LIBRARY
)
