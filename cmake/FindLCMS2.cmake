#.rst:
#  FindLCMS2.cmake
#  ----------------
#  Module to locate Little CMS 2 (aka lcms2) headers and library.
#
#  This module looks for the header `lcms2.h` and the `lcms2` library
#  (static or shared).  If both are found, it defines:
#
#    LCMS2_FOUND          – whether the library was found
#    LCMS2_INCLUDE_DIRS   – directory containing `lcms2.h`
#    LCMS2_LIBRARIES      – full path to the library (debug/release aware)
#
#  It also creates imported target `LCMS2::lcms2` for convenient linking.
#
#  The search can be customised through the following **hint** variables:
#
#    LCMS2_ROOT,  LCMS2_DIR          – installation root where `include/` and
#                                        `lib/` sub-directories reside.
#    LCMS2_INCLUDE_DIR               – explicit include directory path.
#    LCMS2_LIBRARIES / LCMS2_LIBRARY – explicit library path(s).
#
#  ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)

include(FindPackageHandleStandardArgs)
include(SelectLibraryConfigurations)

# ---------------------------------------------------------------------------
# Handle user-provided values first.  Accept both plural and singular vars.
# ---------------------------------------------------------------------------

# Normalise explicit include dir(s)
if(LCMS2_INCLUDE_DIR AND NOT LCMS2_INCLUDE_DIRS)
    set(LCMS2_INCLUDE_DIRS "${LCMS2_INCLUDE_DIR}")
endif()

# Normalise explicit library path(s)
if(LCMS2_LIBRARIES AND NOT LCMS2_LIBRARY)
    list(GET LCMS2_LIBRARIES 0 _lcms2_first)
    set(LCMS2_LIBRARY "${_lcms2_first}")
endif()

if(LCMS2_LIBRARY AND NOT LCMS2_LIBRARIES)
    set(LCMS2_LIBRARIES "${LCMS2_LIBRARY}")
endif()

# If both include and library are already given, we are done.
if(LCMS2_LIBRARIES AND LCMS2_INCLUDE_DIRS)
    set(LCMS2_FOUND TRUE)
else()
    # -----------------------------------------------------------------------
    # Search for header if not given.
    # -----------------------------------------------------------------------
    if(NOT LCMS2_INCLUDE_DIRS)
        find_path(LCMS2_INCLUDE_DIRS NAMES lcms2.h
                  HINTS ${LCMS2_ROOT} $ENV{LCMS2_ROOT} ${LCMS2_DIR} $ENV{LCMS2_DIR}
                  PATH_SUFFIXES include include/lcms2)
    endif()

    # -----------------------------------------------------------------------
    # Search for release / debug libs if not given.
    # -----------------------------------------------------------------------
    if(NOT LCMS2_LIBRARIES)
        find_library(LCMS2_LIBRARY_RELEASE NAMES lcms2 lcms2_static
                     HINTS ${LCMS2_ROOT} $ENV{LCMS2_ROOT} ${LCMS2_DIR} $ENV{LCMS2_DIR}
                     PATH_SUFFIXES lib lib64 libs)
        find_library(LCMS2_LIBRARY_DEBUG NAMES lcms2d lcms2_staticd lcms2d_static
                     HINTS ${LCMS2_ROOT} $ENV{LCMS2_ROOT} ${LCMS2_DIR} $ENV{LCMS2_DIR}
                     PATH_SUFFIXES lib lib64 libs)

        select_library_configurations(LCMS2)  # sets LCMS2_LIBRARY & _LIBRARIES
        set(LCMS2_LIBRARIES "${LCMS2_LIBRARY}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Tell CMake whether we succeeded.
# ---------------------------------------------------------------------------

find_package_handle_standard_args(LCMS2
    REQUIRED_VARS LCMS2_LIBRARIES LCMS2_INCLUDE_DIRS)

# ---------------------------------------------------------------------------
# Create imported target.
# ---------------------------------------------------------------------------

if(LCMS2_FOUND AND NOT TARGET LCMS2::lcms2)
    add_library(LCMS2::lcms2 UNKNOWN IMPORTED)
    set_target_properties(LCMS2::lcms2 PROPERTIES
        IMPORTED_LOCATION "${LCMS2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LCMS2_INCLUDE_DIRS}")
endif() 