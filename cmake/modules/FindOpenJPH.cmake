# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Module to find OPENJPH.
#
# This module will first check for user-provided variables, then look into
# directories defined by:
#   - OPENJPH_ROOT
#
# User-provided variables (checked first):
#   OPENJPH_LIBRARIES   - explicit library paths (semicolon-separated Release;Debug)
#   OPENJPH_INCLUDE_DIR - explicit include directory
#
# This module defines the following variables:
#
# OPENJPH_INCLUDES    - where to find ojph_arg.h
# OPENJPH_LIBRARIES   - list of libraries to link against when using OPENJPH.
# OPENJPH_FOUND       - True if OPENJPH was found.
# OPENJPH_VERSION     - Set to the OPENJPH version found (if available)
#
# It also creates an imported target `openjph` for convenient linking.

include(FindPackageHandleStandardArgs)
include(FindPackageMessage)
include(SelectLibraryConfigurations)

# ---------------------------------------------------------------------------
# Handle user-provided explicit paths first
# ---------------------------------------------------------------------------

if(OPENJPH_LIBRARIES)
    set(OPENJPH_FOUND TRUE)

    # Set includes if provided
    if(OPENJPH_INCLUDE_DIR)
        set(OPENJPH_INCLUDES ${OPENJPH_INCLUDE_DIR})
    endif()

    # Parse the library list - could be "release.lib;debug.lib" format
    list(LENGTH OPENJPH_LIBRARIES _openjph_lib_count)

    if(_openjph_lib_count EQUAL 2)
        # Assume first is Release, second is Debug
        list(GET OPENJPH_LIBRARIES 0 _openjph_lib_release)
        list(GET OPENJPH_LIBRARIES 1 _openjph_lib_debug)
    elseif(_openjph_lib_count EQUAL 1)
        # Single library for both configurations
        list(GET OPENJPH_LIBRARIES 0 _openjph_lib_release)
        set(_openjph_lib_debug ${_openjph_lib_release})
    else()
        # Multiple libraries - try to identify by name pattern
        set(_openjph_lib_release "")
        set(_openjph_lib_debug "")
        foreach(_lib ${OPENJPH_LIBRARIES})
            string(TOLOWER "${_lib}" _lib_lower)
            if(_lib_lower MATCHES "_d\\.lib$" OR _lib_lower MATCHES "d\\.lib$" OR _lib_lower MATCHES "debug")
                set(_openjph_lib_debug ${_lib})
            else()
                set(_openjph_lib_release ${_lib})
            endif()
        endforeach()
        # Fallback if pattern matching failed
        if(NOT _openjph_lib_release AND OPENJPH_LIBRARIES)
            list(GET OPENJPH_LIBRARIES 0 _openjph_lib_release)
        endif()
        if(NOT _openjph_lib_debug)
            set(_openjph_lib_debug ${_openjph_lib_release})
        endif()
    endif()

    # Create imported target
    if(NOT TARGET openjph)
        add_library(openjph UNKNOWN IMPORTED)

        if(OPENJPH_INCLUDES)
            set_target_properties(openjph PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${OPENJPH_INCLUDES}"
            )
        endif()

        if(_openjph_lib_release AND _openjph_lib_debug AND NOT _openjph_lib_release STREQUAL _openjph_lib_debug)
            set_target_properties(openjph PROPERTIES
                IMPORTED_LOCATION_RELEASE "${_openjph_lib_release}"
                IMPORTED_LOCATION_DEBUG "${_openjph_lib_debug}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${_openjph_lib_release}"
                IMPORTED_LOCATION_MINSIZEREL "${_openjph_lib_release}"
            )
            # Set default location for configs that don't match
            set_target_properties(openjph PROPERTIES
                IMPORTED_LOCATION "${_openjph_lib_release}"
            )
        else()
            set_target_properties(openjph PROPERTIES
                IMPORTED_LOCATION "${_openjph_lib_release}"
            )
        endif()
    endif()

    if(NOT OPENJPH_FIND_QUIETLY)
        FIND_PACKAGE_MESSAGE(OPENJPH
            "Found OPENJPH via user-provided paths: ${OPENJPH_LIBRARIES}"
            "[${OPENJPH_INCLUDES}][${OPENJPH_LIBRARIES}]"
        )
    endif()

else()
    # ---------------------------------------------------------------------------
    # Auto-detection via pkg-config
    # ---------------------------------------------------------------------------

    if(DEFINED OPENJPH_ROOT)
        set(_openjph_pkgconfig_path "${OPENJPH_ROOT}/lib/pkgconfig")
        if(EXISTS "${_openjph_pkgconfig_path}")
            set(ENV{PKG_CONFIG_PATH} "${_openjph_pkgconfig_path}:$ENV{PKG_CONFIG_PATH}")
        endif()
    endif()

    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(OPENJPH_PC QUIET openjph)
    endif()

    if(OPENJPH_PC_FOUND)
        set(OPENJPH_FOUND TRUE)
        set(OPENJPH_VERSION ${OPENJPH_PC_VERSION})
        set(OPENJPH_INCLUDES ${OPENJPH_PC_INCLUDE_DIRS})
        set(OPENJPH_LIBRARY_DIRS ${OPENJPH_PC_LIBDIR})
        set(OPENJPH_LIBRARIES ${OPENJPH_PC_LIBRARIES})

        # Create imported target from pkg-config results
        if(NOT TARGET openjph)
            add_library(openjph INTERFACE IMPORTED)
            set_target_properties(openjph PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${OPENJPH_INCLUDES}"
                INTERFACE_LINK_LIBRARIES "${OPENJPH_LIBRARIES}"
                INTERFACE_LINK_DIRECTORIES "${OPENJPH_LIBRARY_DIRS}"
            )
        endif()

        if(NOT OPENJPH_FIND_QUIETLY)
            FIND_PACKAGE_MESSAGE(OPENJPH
                "Found OPENJPH via pkg-config: v${OPENJPH_VERSION} ${OPENJPH_LIBRARIES}"
                "[${OPENJPH_INCLUDES}][${OPENJPH_LIBRARIES}]"
            )
        endif()
    else()
        # ---------------------------------------------------------------------------
        # Fallback: Manual search
        # ---------------------------------------------------------------------------

        # Search paths
        set(_openjph_search_paths
            ${OPENJPH_ROOT}
            ${CMAKE_PREFIX_PATH}
            /usr/local
            /usr
        )

        # Find header
        find_path(OPENJPH_INCLUDE_DIR
            NAMES openjph/ojph_arg.h ojph_arg.h
            HINTS ${_openjph_search_paths}
            PATH_SUFFIXES include
        )

        # Find library
        find_library(OPENJPH_LIBRARY_RELEASE
            NAMES openjph openjph.0.21 libopenjph
            HINTS ${_openjph_search_paths}
            PATH_SUFFIXES lib lib64
        )

        find_library(OPENJPH_LIBRARY_DEBUG
            NAMES openjphd openjph_d openjph.0.21_d libopenjphd
            HINTS ${_openjph_search_paths}
            PATH_SUFFIXES lib lib64
        )

        if(OPENJPH_INCLUDE_DIR AND OPENJPH_LIBRARY_RELEASE)
            set(OPENJPH_FOUND TRUE)
            set(OPENJPH_INCLUDES ${OPENJPH_INCLUDE_DIR})
            set(OPENJPH_LIBRARIES ${OPENJPH_LIBRARY_RELEASE})

            # Create imported target
            if(NOT TARGET openjph)
                add_library(openjph UNKNOWN IMPORTED)
                set_target_properties(openjph PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${OPENJPH_INCLUDES}"
                )

                if(OPENJPH_LIBRARY_DEBUG)
                    set_target_properties(openjph PROPERTIES
                        IMPORTED_LOCATION_RELEASE "${OPENJPH_LIBRARY_RELEASE}"
                        IMPORTED_LOCATION_DEBUG "${OPENJPH_LIBRARY_DEBUG}"
                        IMPORTED_LOCATION "${OPENJPH_LIBRARY_RELEASE}"
                    )
                else()
                    set_target_properties(openjph PROPERTIES
                        IMPORTED_LOCATION "${OPENJPH_LIBRARY_RELEASE}"
                    )
                endif()
            endif()

            if(NOT OPENJPH_FIND_QUIETLY)
                FIND_PACKAGE_MESSAGE(OPENJPH
                    "Found OPENJPH via manual search: ${OPENJPH_LIBRARIES}"
                    "[${OPENJPH_INCLUDES}][${OPENJPH_LIBRARIES}]"
                )
            endif()
        else()
            set(OPENJPH_FOUND FALSE)
            set(OPENJPH_VERSION 0.0.0)
            set(OPENJPH_INCLUDES "")
            set(OPENJPH_LIBRARIES "")
            if(NOT OPENJPH_FIND_QUIETLY)
                FIND_PACKAGE_MESSAGE(OPENJPH
                    "Could not find OPENJPH"
                    "[${OPENJPH_INCLUDES}][${OPENJPH_LIBRARIES}]"
                )
            endif()
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Standard CMake package handling
# ---------------------------------------------------------------------------

# Use lowercase 'openjph' to match the package name when called via find_dependency(openjph)
find_package_handle_standard_args(openjph
    REQUIRED_VARS OPENJPH_FOUND
    VERSION_VAR OPENJPH_VERSION
)

# Set all case variants for maximum compatibility
if(OPENJPH_FOUND)
    set(openjph_FOUND TRUE CACHE BOOL "OpenJPH found" FORCE)
    set(OpenJPH_FOUND TRUE CACHE BOOL "OpenJPH found" FORCE)
    set(OPENJPH_FOUND TRUE CACHE BOOL "OpenJPH found" FORCE)
endif()

# Mark cache variables as advanced
mark_as_advanced(
    OPENJPH_INCLUDE_DIR
    OPENJPH_LIBRARY_RELEASE
    OPENJPH_LIBRARY_DEBUG
    openjph_FOUND
    OpenJPH_FOUND
)
