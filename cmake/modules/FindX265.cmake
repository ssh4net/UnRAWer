# FindX265.cmake
# Find the x265 HEVC encoder library
#
# This module defines:
#  X265_FOUND - System has x265
#  X265_INCLUDE_DIR - The x265 include directory
#  X265_LIBRARIES - The libraries needed to use x265
#  X265::X265 - Imported target for x265
#
# On Windows with MSVC, this module automatically selects the correct library variant:
#  - Static runtime (/MT, /MTd): x265-static.lib, x265-staticd.lib
#  - Dynamic runtime (/MD, /MDd): libx265.lib, libx265d.lib

# Find include directory
find_path(X265_INCLUDE_DIR
    NAMES x265.h
    HINTS
        ${CMAKE_PREFIX_PATH}/include
        $ENV{X265_ROOT}/include
    PATHS
        /usr/include
        /usr/local/include
    DOC "Path to x265 include directory"
)

# Determine library names based on platform and runtime library
if(MSVC)
    # Detect MSVC runtime library type from CMAKE_MSVC_RUNTIME_LIBRARY variable
    # This should be set in the main CMakeLists.txt before find_package(X265)
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(MSVC_RUNTIME ${CMAKE_MSVC_RUNTIME_LIBRARY})
    else()
        # Default assumption: static runtime (/MT)
        set(MSVC_RUNTIME "MultiThreaded")
        message(STATUS "FindX265: CMAKE_MSVC_RUNTIME_LIBRARY not set, defaulting to static runtime")
    endif()

    # Determine if using static runtime (/MT, /MTd) or dynamic runtime (/MD, /MDd)
    # The runtime library string may contain generator expressions like:
    #   "MultiThreaded$<$<CONFIG:Debug>:Debug>" for /MT and /MTd
    #   "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>" for /MD and /MDd
    if(MSVC_RUNTIME MATCHES "MultiThreadedDLL")
        # Dynamic runtime (/MD, /MDd)
        set(X265_LIB_NAME_RELEASE "libx265")
        set(X265_LIB_NAME_DEBUG "libx265d")
        message(STATUS "FindX265: Detected dynamic runtime (/MD) - searching for libx265.lib/libx265d.lib")
    else()
        # Static runtime (/MT, /MTd) - this is the default
        set(X265_LIB_NAME_RELEASE "x265-static")
        set(X265_LIB_NAME_DEBUG "x265-staticd")
        message(STATUS "FindX265: Detected static runtime (/MT) - searching for x265-static.lib/x265-staticd.lib")
    endif()
else()
    # Unix/Linux: typically uses libx265.a or libx265.so
    set(X265_LIB_NAME_RELEASE "x265")
    set(X265_LIB_NAME_DEBUG "x265")
endif()

# Find Release library
find_library(X265_LIBRARY_RELEASE
    NAMES ${X265_LIB_NAME_RELEASE}
    HINTS
        ${CMAKE_PREFIX_PATH}/lib
        $ENV{X265_ROOT}/lib
    PATHS
        /usr/lib
        /usr/local/lib
    DOC "Path to x265 release library"
)

# Find Debug library
find_library(X265_LIBRARY_DEBUG
    NAMES ${X265_LIB_NAME_DEBUG}
    HINTS
        ${CMAKE_PREFIX_PATH}/lib
        $ENV{X265_ROOT}/lib
    PATHS
        /usr/lib
        /usr/local/lib
    DOC "Path to x265 debug library"
)

# Handle REQUIRED and QUIET arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(X265
    REQUIRED_VARS X265_INCLUDE_DIR
    FOUND_VAR X265_FOUND
)

# Set X265_LIBRARIES based on what was found
if(X265_LIBRARY_RELEASE AND X265_LIBRARY_DEBUG)
    set(X265_LIBRARIES
        optimized ${X265_LIBRARY_RELEASE}
        debug ${X265_LIBRARY_DEBUG}
    )
elseif(X265_LIBRARY_RELEASE)
    set(X265_LIBRARIES ${X265_LIBRARY_RELEASE})
elseif(X265_LIBRARY_DEBUG)
    set(X265_LIBRARIES ${X265_LIBRARY_DEBUG})
else()
    set(X265_LIBRARIES "")
endif()

# Create imported target
if(X265_FOUND AND NOT TARGET X265::X265)
    add_library(X265::X265 UNKNOWN IMPORTED)

    set_target_properties(X265::X265 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${X265_INCLUDE_DIR}"
    )

    # Set library locations for both configurations
    if(X265_LIBRARY_RELEASE)
        set_property(TARGET X265::X265 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(X265::X265 PROPERTIES
            IMPORTED_LOCATION_RELEASE "${X265_LIBRARY_RELEASE}"
        )
    endif()

    if(X265_LIBRARY_DEBUG)
        set_property(TARGET X265::X265 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(X265::X265 PROPERTIES
            IMPORTED_LOCATION_DEBUG "${X265_LIBRARY_DEBUG}"
        )
    endif()

    # Fallback for single-configuration generators or if only one variant exists
    if(X265_LIBRARY_RELEASE AND NOT X265_LIBRARY_DEBUG)
        set_target_properties(X265::X265 PROPERTIES
            IMPORTED_LOCATION "${X265_LIBRARY_RELEASE}"
        )
    elseif(X265_LIBRARY_DEBUG AND NOT X265_LIBRARY_RELEASE)
        set_target_properties(X265::X265 PROPERTIES
            IMPORTED_LOCATION "${X265_LIBRARY_DEBUG}"
        )
    endif()
endif()

# Mark advanced variables
mark_as_advanced(
    X265_INCLUDE_DIR
    X265_LIBRARY_RELEASE
    X265_LIBRARY_DEBUG
)

# Report findings
if(X265_FOUND)
    message(STATUS "Found x265: ${X265_LIBRARIES}")
else()
    if(X265_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find x265 library")
    endif()
endif()
