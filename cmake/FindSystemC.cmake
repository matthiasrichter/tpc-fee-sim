# - Find SystemC
# This module finds if SystemC is installed and determines where the
# include files and libraries are. This code sets the following
# variables: (from kernel/sc_ver.h)
#
#  SystemC_VERSION_MAJOR      = The major version of the package found.
#  SystemC_VERSION_MINOR      = The minor version of the package found.
#  SystemC_VERSION_REV        = The patch version of the package found.
#  SystemC_VERSION            = This is set to: $major.$minor.$rev
#
# The minimum required version of SystemC can be specified using the
# standard CMake syntax, e.g. FIND_PACKAGE(SystemC 2.2)
#
# For these components the following variables are set:
#
#  SYSTEMC_INCDIR             - Full paths to include directory.
#  SYSTEMC_LIBDIR             - Full paths to library directory.
#  SYSTEMC_LIBRARIES          - Link libraries.
#
# Example Usages:
#  FIND_PACKAGE(SystemC)
#  FIND_PACKAGE(SystemC 2.3)
#
# Custom path to SystemC installation directory can be specified using
# option -DSYSTEMC_PREFIX=<path to installation> when invoking cmake

message(STATUS "Searching for SystemC")

# custom hints to find SystemC installation
# option 1) -DSYSTEMC_PREFIX=<path to installation> flag
# option 2) environment variable SYSTEMC_PREFIX
# include and library paths are put together in one list for simplicity
set(SEARCH_HINTS_EXTENSIONS
  include
  lib
  lib-linux
  lib-linux64
  lib-macos
  lib-macosx64
  )

SET(SYSTEMC_SEARCH_PATHS
  ${SYSTEMC_SEARCH_PATHS}
  /usr/include/systemc
  )

if(NOT DEFINED SYSTEMC_PREFIX)
  SET(SYSTEMC_PREFIX $ENV{SYSTEMC_PREFIX})
endif(NOT DEFINED SYSTEMC_PREFIX)

foreach(extension ${SEARCH_HINTS_EXTENSIONS})
if(DEFINED SYSTEMC_PREFIX)
SET(SYSTEMC_SEARCH_HINTS
  ${SYSTEMC_SEARCH_HINTS}
  ${SYSTEMC_PREFIX}/${extension}
  )
endif(DEFINED SYSTEMC_PREFIX)

# Possibe system wide installation paths for searches
SET(SYSTEMC_SEARCH_PATHS
  ${SYSTEMC_SEARCH_PATHS}
  /usr/${extension}
  /usr/local/${extension}
  )
endforeach(extension)

FIND_FILE(_SYSTEMC_VERSION_FILE
  NAMES sc_ver.h
  HINTS ${SYSTEMC_SEARCH_HINTS}
  PATHS ${SYSTEMC_SEARCH_PATHS}
  PATH_SUFFIXES sysc/kernel
)

if (EXISTS ${_SYSTEMC_VERSION_FILE})
# extract version number from the version header file
EXEC_PROGRAM("cat ${_SYSTEMC_VERSION_FILE} |grep '#define SC_VERSION_MAJOR' "
             OUTPUT_VARIABLE SYSTEMC_MAJOR)
string(REGEX REPLACE ".* \([0-9]\)*$" "\\1" SYSTEMC_MAJOR ${SYSTEMC_MAJOR})
EXEC_PROGRAM("cat ${_SYSTEMC_VERSION_FILE} |grep '#define SC_VERSION_MINOR' "
             OUTPUT_VARIABLE SYSTEMC_MINOR)
string(REGEX REPLACE ".* \([0-9]\)*$" "\\1" SYSTEMC_MINOR ${SYSTEMC_MINOR})
EXEC_PROGRAM("cat ${_SYSTEMC_VERSION_FILE} |grep '#define SC_VERSION_PATCH' "
             OUTPUT_VARIABLE SYSTEMC_PATCH)
string(REGEX REPLACE ".* \([0-9]\)*$" "\\1" SYSTEMC_PATCH ${SYSTEMC_PATCH})
set(SYSTEMC_VERSION ${SYSTEMC_MAJOR}.${SYSTEMC_MINOR}.${SYSTEMC_PATCH})
endif (EXISTS ${_SYSTEMC_VERSION_FILE})

FIND_PATH(SYSTEMC_INCDIR
  NAMES systemc.h
  HINTS ${SYSTEMC_SEARCH_HINTS}
  PATHS ${SYSTEMC_SEARCH_PATHS}
)

set(SYSTEMC_SEARCH_LIBRARY systemc)

FIND_LIBRARY(SYSTEMC_LIBRARY
  NAMES ${SYSTEMC_SEARCH_LIBRARY}
  HINTS ${SYSTEMC_SEARCH_HINTS}
  PATHS ${SYSTEMC_SEARCH_PATHS}
  DOC "SystemC libraries)"
)

GET_FILENAME_COMPONENT(SYSTEMC_LIBDIR
  ${SYSTEMC_LIBRARY}
  DIRECTORY
)

# for the moment there is only one SystemC library
GET_FILENAME_COMPONENT(SYSTEMC_LIBRARIES
  ${SYSTEMC_LIBRARY}
  NAME
)

if (EXISTS ${SYSTEMC_INCDIR} AND
    EXISTS ${SYSTEMC_LIBDIR})
    set(SYSTEMC_FOUND 1)
else()
# TODO: probably some error message
endif (EXISTS ${SYSTEMC_INCDIR} AND
       EXISTS ${SYSTEMC_LIBDIR})

if (${SYSTEMC_FOUND})
message(STATUS "SystemC version = ${SYSTEMC_VERSION}")
message(STATUS "SYSTEMC_INCDIR:     ${SYSTEMC_INCDIR}")
message(STATUS "SYSTEMC_LIBDIR:     ${SYSTEMC_LIBDIR}")
message(STATUS "SystemC library:    ${SYSTEMC_LIBRARIES}")
else()
message(STATUS "... not found, you might want to specify -DSYSTEMC_PREFIX=<installation>")
endif (${SYSTEMC_FOUND})
