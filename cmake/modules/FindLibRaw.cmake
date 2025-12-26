# - Find LibRaw
# Find the LibRaw library <http://www.libraw.org>
#
# Usage:
#   find_package(LibRaw [REQUIRED] [QUIET])
#
# It defines the following variables:
#   LibRaw_FOUND          - True if LibRaw is found.
#   LibRaw_INCLUDE_DIR    - LibRaw include directory.
#   LibRaw::LibRaw        - Imported interface target to link against.
#
# User can manually provide:
#   LibRaw_LIBRARIES       - List of release libraries (e.g. "raw.lib;raw_r.lib")
#   LibRaw_LIBRARIES_DEBUG - List of debug libraries (e.g. "rawd.lib;raw_rd.lib")
#   LibRaw_INCLUDE_DIR     - Include directory
#

cmake_minimum_required(VERSION 3.10)

# ---------------------------------------------------------------------------
# 1. Check for manual variables
# ---------------------------------------------------------------------------
if(LibRaw_LIBRARIES AND LibRaw_INCLUDE_DIR)
    set(LibRaw_FOUND TRUE)
    set(LibRaw_LIBRARIES_FOUND ${LibRaw_LIBRARIES})
    if(LibRaw_LIBRARIES_DEBUG)
        set(LibRaw_LIBRARIES_DEBUG_FOUND ${LibRaw_LIBRARIES_DEBUG})
    endif()
else()
    # ---------------------------------------------------------------------------
    # 2. Search for LibRaw
    # ---------------------------------------------------------------------------
    find_path(LibRaw_INCLUDE_DIR NAMES libraw/libraw.h)

    # Search paths
    set(_libraw_search_lib 
        ${CMAKE_PREFIX_PATH}/lib 
        /usr/lib 
        /usr/local/lib
    )

    # Release
    find_library(LibRaw_LIBRARY NAMES raw libraw raw_static libraw_static HINTS ${_libraw_search_lib})
    find_library(LibRaw_r_LIBRARY NAMES raw_r libraw_r HINTS ${_libraw_search_lib})

    # Debug
    find_library(LibRaw_LIBRARY_DEBUG NAMES rawd librawd raw_staticd libraw_staticd HINTS ${_libraw_search_lib})
    find_library(LibRaw_r_LIBRARY_DEBUG NAMES raw_rd libraw_rd HINTS ${_libraw_search_lib})

    # Construct lists
    set(LibRaw_LIBRARIES_FOUND "")
    if(LibRaw_LIBRARY)
        list(APPEND LibRaw_LIBRARIES_FOUND ${LibRaw_LIBRARY})
    endif()
    if(LibRaw_r_LIBRARY)
        list(APPEND LibRaw_LIBRARIES_FOUND ${LibRaw_r_LIBRARY})
    endif()

    set(LibRaw_LIBRARIES_DEBUG_FOUND "")
    if(LibRaw_LIBRARY_DEBUG)
        list(APPEND LibRaw_LIBRARIES_DEBUG_FOUND ${LibRaw_LIBRARY_DEBUG})
    endif()
    if(LibRaw_r_LIBRARY_DEBUG)
        list(APPEND LibRaw_LIBRARIES_DEBUG_FOUND ${LibRaw_r_LIBRARY_DEBUG})
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(LibRaw 
        REQUIRED_VARS LibRaw_INCLUDE_DIR LibRaw_LIBRARIES_FOUND
    )
endif()

# ---------------------------------------------------------------------------
# 3. Create Imported Target
# ---------------------------------------------------------------------------
if(LibRaw_FOUND AND NOT TARGET LibRaw::LibRaw)
    add_library(LibRaw::LibRaw INTERFACE IMPORTED)
    set_target_properties(LibRaw::LibRaw PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LibRaw_INCLUDE_DIR}"
    )

    if(LibRaw_LIBRARIES_DEBUG_FOUND)
        # Use generator expression for configuration specific linking
        set_property(TARGET LibRaw::LibRaw PROPERTY INTERFACE_LINK_LIBRARIES
            "$<$<CONFIG:Debug>:${LibRaw_LIBRARIES_DEBUG_FOUND}>"
            "$<$<CONFIG:Release>:${LibRaw_LIBRARIES_FOUND}>"
            # Fallback for other configs to Release? Or explicit?
            # Usually RelWithDebInfo uses Release libs or separate ones. 
            # We'll map Release to Release/RelWithDebInfo/MinSizeRel and Debug to Debug.
            "$<$<CONFIG:RelWithDebInfo>:${LibRaw_LIBRARIES_FOUND}>"
            "$<$<CONFIG:MinSizeRel>:${LibRaw_LIBRARIES_FOUND}>"
        )
    else()
        set_property(TARGET LibRaw::LibRaw PROPERTY INTERFACE_LINK_LIBRARIES "${LibRaw_LIBRARIES_FOUND}")
    endif()
endif()