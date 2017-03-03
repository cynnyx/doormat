# - Find dl library
# Find the native DL includes and library
#
#  DL_INCLUDE_DIR - where to find dlfcn.h, etc.
#  DL_LIBRARIES   - List of libraries when using dl.
#  DL_FOUND       - True if dl found.


IF (DL_INCLUDE_DIR)
  # Already in cache, be silent
  SET(DL_FIND_QUIETLY TRUE)
ENDIF (DL_INCLUDE_DIR)

FIND_PATH(DL_INCLUDE_DIR dlfcn.h)

SET(DL_NAMES dl libdl ltdl libltdl)
FIND_LIBRARY(DL_LIBRARY NAMES ${DL_NAMES} )

# handle the QUIETLY and REQUIRED arguments and set DL_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DL DEFAULT_MSG DL_LIBRARY DL_INCLUDE_DIR)

IF(DL_FOUND)
  SET( DL_LIBRARIES ${DL_LIBRARY} )
ELSE(DL_FOUND)
  SET( DL_LIBRARIES )
ENDIF(DL_FOUND)

MARK_AS_ADVANCED( DL_LIBRARY DL_INCLUDE_DIR )
