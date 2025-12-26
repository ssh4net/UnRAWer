#.rst:
#  FindDNG.cmake
#  -------------
#  Module to locate Adobe DNG SDK headers and libraries.
#
#  Usage:
#    find_package(DNG [REQUIRED] [QUIET])
#
#  If found, it creates imported target `DNG::SDK`.
#
#  Variables:
#    DNG_SDK_DIR          – DNG SDK root directory (contains source/ folder)
#    DNG_INCLUDE_DIR      – explicit include directory path
#    DNG_LIBRARY          – explicit Release library path
#    DNG_LIBRARY_DEBUG    – explicit Debug library path
#
#  ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)

include(FindPackageHandleStandardArgs)

# ---------------------------------------------------------------------------
# Handle user-provided explicit library paths first
# ---------------------------------------------------------------------------

if(DNG_LIBRARY)
    # Use explicitly provided libraries
    set(DNG_LIBRARIES_FOUND ${DNG_LIBRARY})
    if(DNG_LIBRARY_DEBUG)
        set(DNG_LIBRARIES_DEBUG_FOUND ${DNG_LIBRARY_DEBUG})
    endif()

    # Handle include directory - try explicit, then derive from DNG_SDK_DIR
    if(DNG_INCLUDE_DIR)
        set(DNG_INCLUDE_DIRS ${DNG_INCLUDE_DIR})
    elseif(DNG_SDK_DIR)
        # DNG SDK typically has headers in source/ directory
        if(EXISTS "${DNG_SDK_DIR}/dng_sdk.h")
            set(DNG_INCLUDE_DIRS "${DNG_SDK_DIR}")
        elseif(EXISTS "${DNG_SDK_DIR}/source/dng_sdk.h")
            set(DNG_INCLUDE_DIRS "${DNG_SDK_DIR}/source")
        elseif(EXISTS "${DNG_SDK_DIR}/include/dng_sdk.h")
            set(DNG_INCLUDE_DIRS "${DNG_SDK_DIR}/include")
        else()
            # Just use the provided directory
            set(DNG_INCLUDE_DIRS "${DNG_SDK_DIR}")
        endif()
    endif()
else()
    # ---------------------------------------------------------------------------
    # Set up search paths
    # ---------------------------------------------------------------------------

    if(DNG_SDK_DIR)
        # User specified DNG SDK directory
        if(DNG_INCLUDE_DIR)
            set(_dng_include_paths ${DNG_INCLUDE_DIR})
        else()
            set(_dng_include_paths
                "${DNG_SDK_DIR}/source"
                "${DNG_SDK_DIR}/include"
                "${DNG_SDK_DIR}"
            )
        endif()

        set(_dng_lib_paths
            "${DNG_SDK_DIR}/lib"
            "${DNG_SDK_DIR}/libraries"
            "${DNG_SDK_DIR}/build/lib"
            "${DNG_SDK_DIR}/projects/win/x64/Release"
            "${DNG_SDK_DIR}/projects/win/x64/Debug"
        )
    else()
        # Default search paths
        set(_dng_include_paths
            /usr/include/dng_sdk
            /usr/local/include/dng_sdk
            ${CMAKE_PREFIX_PATH}/include/dng_sdk
            ${CMAKE_PREFIX_PATH}/include
        )

        set(_dng_lib_paths
            /usr/lib
            /usr/local/lib
            ${CMAKE_PREFIX_PATH}/lib
        )
    endif()

    # ---------------------------------------------------------------------------
    # Search for headers and libraries
    # ---------------------------------------------------------------------------

    find_path(DNG_INCLUDE_DIR
        NAMES dng_sdk.h dng_version.h dng_types.h
        PATHS ${_dng_include_paths}
        NO_DEFAULT_PATH
    )

    find_library(DNG_LIBRARY
        NAMES dng_sdk libdng_sdk
        PATHS ${_dng_lib_paths}
        NO_DEFAULT_PATH
    )

    find_library(DNG_LIBRARY_DEBUG
        NAMES dng_sdkd libdng_sdkd dng_sdk_d libdng_sdk_d
        PATHS ${_dng_lib_paths}
        NO_DEFAULT_PATH
    )

    if(DNG_LIBRARY)
        set(DNG_LIBRARIES_FOUND ${DNG_LIBRARY})
    endif()
    if(DNG_LIBRARY_DEBUG)
        set(DNG_LIBRARIES_DEBUG_FOUND ${DNG_LIBRARY_DEBUG})
    endif()
    if(DNG_INCLUDE_DIR)
        set(DNG_INCLUDE_DIRS ${DNG_INCLUDE_DIR})
    endif()
endif()

# ---------------------------------------------------------------------------
# Set DNG found status and create target
# ---------------------------------------------------------------------------

if(DNG_LIBRARIES_FOUND AND DNG_INCLUDE_DIRS)
    set(DNG_FOUND TRUE)

    if(NOT TARGET DNG::SDK)
        add_library(DNG::SDK UNKNOWN IMPORTED)
        set_target_properties(DNG::SDK PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${DNG_INCLUDE_DIRS}"
        )
        if(DNG_LIBRARIES_DEBUG_FOUND)
            set_target_properties(DNG::SDK PROPERTIES
                IMPORTED_LOCATION_RELEASE "${DNG_LIBRARIES_FOUND}"
                IMPORTED_LOCATION_DEBUG "${DNG_LIBRARIES_DEBUG_FOUND}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${DNG_LIBRARIES_FOUND}"
                IMPORTED_LOCATION_MINSIZEREL "${DNG_LIBRARIES_FOUND}"
                IMPORTED_LOCATION "${DNG_LIBRARIES_FOUND}"
            )
        else()
            set_target_properties(DNG::SDK PROPERTIES
                IMPORTED_LOCATION "${DNG_LIBRARIES_FOUND}"
            )
        endif()
    endif()

    message(STATUS "Found DNG SDK:")
    message(STATUS "  Include: ${DNG_INCLUDE_DIRS}")
    message(STATUS "  Library: ${DNG_LIBRARIES_FOUND}")
    if(DNG_LIBRARIES_DEBUG_FOUND)
        message(STATUS "  Library (Debug): ${DNG_LIBRARIES_DEBUG_FOUND}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Tell CMake whether we succeeded
# ---------------------------------------------------------------------------

find_package_handle_standard_args(DNG
    REQUIRED_VARS DNG_LIBRARIES_FOUND DNG_INCLUDE_DIRS
)

mark_as_advanced(
    DNG_INCLUDE_DIR
    DNG_LIBRARY
    DNG_LIBRARY_DEBUG
)
