#.rst:
#  FindXMP.cmake
#  -------------
#  Module to locate Adobe XMP SDK headers and libraries.
#
#  This module looks for the XMP SDK Core and Files libraries and creates imported targets.
#  If found, it defines:
#
#    XMP_FOUND               – whether the library was found
#    XMP_INCLUDE_DIRS        – directory containing XMP SDK headers
#    XMP_CORE_LIBRARY        – path to XMP Core library
#    XMP_FILES_LIBRARY       – path to XMP Files library
#    XMP_LIBRARIES           – list of all XMP libraries
#
#  It also creates imported targets `XMP::Core` and `XMP::Files` for convenient linking,
#  or `XMP::XMP` when using the generic XMP_LIBRARIES variable.
#
#  The search can be customized through the following **cache** variables:
#
#    XMP_SDK / XMP_SDK_DIR    – XMP SDK root directory (both work)
#    XMP_INCLUDE_DIR          – explicit include directory path
#    XMP_LIBRARY_DIR          – explicit library directory path
#    XMP_Core_LIB             – explicit XMP Core Release library path
#    XMP_Core_LIB_DEBUG       – explicit XMP Core Debug library path
#    XMP_Files_LIB            – explicit XMP Files Release library path
#    XMP_Files_LIB_DEBUG      – explicit XMP Files Debug library path
#    XMP_LIBRARIES            – explicit list of libraries (semicolon-separated, Release;Debug or Core;Files format)
#    XMP_LIBRARIES_DEBUG      – explicit list of debug libraries (optional)
#
#  ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)

# Support both XMP_SDK and XMP_SDK_DIR
if(XMP_SDK AND NOT XMP_SDK_DIR)
    set(XMP_SDK_DIR ${XMP_SDK})
endif()

include(FindPackageHandleStandardArgs)

# ---------------------------------------------------------------------------
# Handle user-provided explicit library paths first (XMP_LIBRARIES)
# ---------------------------------------------------------------------------

if(XMP_LIBRARIES AND NOT XMP_Core_LIB AND NOT XMP_Files_LIB)
    # User provided XMP_LIBRARIES as a list - parse it
    set(XMP_FOUND TRUE)

    list(LENGTH XMP_LIBRARIES _xmp_lib_count)

    # Categorize libraries into Release/Debug and Core/Files
    set(_xmp_core_release "")
    set(_xmp_core_debug "")
    set(_xmp_files_release "")
    set(_xmp_files_debug "")

    foreach(_lib ${XMP_LIBRARIES})
        string(TOLOWER "${_lib}" _lib_lower)
        get_filename_component(_lib_name "${_lib}" NAME)
        string(TOLOWER "${_lib_name}" _lib_name_lower)

        # Determine if Core or Files
        if(_lib_name_lower MATCHES "core")
            # Determine if Debug or Release
            if(_lib_name_lower MATCHES "debug")
                set(_xmp_core_debug "${_lib}")
            else()
                set(_xmp_core_release "${_lib}")
            endif()
        elseif(_lib_name_lower MATCHES "files")
            if(_lib_name_lower MATCHES "debug")
                set(_xmp_files_debug "${_lib}")
            else()
                set(_xmp_files_release "${_lib}")
            endif()
        endif()
    endforeach()

    # Set include directory if provided
    if(XMP_INCLUDE_DIR)
        set(XMP_INCLUDE_DIRS ${XMP_INCLUDE_DIR})
    elseif(XMP_SDK_DIR)
        # Try common XMP include paths
        if(EXISTS "${XMP_SDK_DIR}/public/include")
            set(XMP_INCLUDE_DIRS "${XMP_SDK_DIR}/public/include")
        elseif(EXISTS "${XMP_SDK_DIR}/include")
            set(XMP_INCLUDE_DIRS "${XMP_SDK_DIR}/include")
        endif()
    endif()

    # Create XMP::Core target if we found Core libraries
    if(_xmp_core_release OR _xmp_core_debug)
        if(NOT TARGET XMP::Core)
            add_library(XMP::Core UNKNOWN IMPORTED)
            if(XMP_INCLUDE_DIRS)
                set_target_properties(XMP::Core PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIRS}"
                )
            endif()
            if(_xmp_core_release AND _xmp_core_debug)
                set_target_properties(XMP::Core PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${_xmp_core_release}"
                    IMPORTED_LOCATION_DEBUG "${_xmp_core_debug}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${_xmp_core_release}"
                    IMPORTED_LOCATION_MINSIZEREL "${_xmp_core_release}"
                    IMPORTED_LOCATION "${_xmp_core_release}"
                )
            elseif(_xmp_core_release)
                set_target_properties(XMP::Core PROPERTIES
                    IMPORTED_LOCATION "${_xmp_core_release}"
                )
            else()
                set_target_properties(XMP::Core PROPERTIES
                    IMPORTED_LOCATION "${_xmp_core_debug}"
                )
            endif()
        endif()
    endif()

    # Create XMP::Files target if we found Files libraries
    if(_xmp_files_release OR _xmp_files_debug)
        if(NOT TARGET XMP::Files)
            add_library(XMP::Files UNKNOWN IMPORTED)
            if(XMP_INCLUDE_DIRS)
                set_target_properties(XMP::Files PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIRS}"
                )
            endif()
            if(_xmp_files_release AND _xmp_files_debug)
                set_target_properties(XMP::Files PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${_xmp_files_release}"
                    IMPORTED_LOCATION_DEBUG "${_xmp_files_debug}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${_xmp_files_release}"
                    IMPORTED_LOCATION_MINSIZEREL "${_xmp_files_release}"
                    IMPORTED_LOCATION "${_xmp_files_release}"
                )
            elseif(_xmp_files_release)
                set_target_properties(XMP::Files PROPERTIES
                    IMPORTED_LOCATION "${_xmp_files_release}"
                )
            else()
                set_target_properties(XMP::Files PROPERTIES
                    IMPORTED_LOCATION "${_xmp_files_debug}"
                )
            endif()
        endif()
    endif()

    # Also create a combined XMP::XMP target using INTERFACE
    if(NOT TARGET XMP::XMP)
        add_library(XMP::XMP INTERFACE IMPORTED)
        set(_xmp_link_libs "")
        if(TARGET XMP::Core)
            list(APPEND _xmp_link_libs XMP::Core)
        endif()
        if(TARGET XMP::Files)
            list(APPEND _xmp_link_libs XMP::Files)
        endif()
        if(_xmp_link_libs)
            set_target_properties(XMP::XMP PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_xmp_link_libs}"
            )
        endif()
        if(XMP_INCLUDE_DIRS)
            set_target_properties(XMP::XMP PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIRS}"
            )
        endif()
    endif()

    message(STATUS "Found XMP via user-provided XMP_LIBRARIES:")
    message(STATUS "  Core Release: ${_xmp_core_release}")
    message(STATUS "  Core Debug: ${_xmp_core_debug}")
    message(STATUS "  Files Release: ${_xmp_files_release}")
    message(STATUS "  Files Debug: ${_xmp_files_debug}")
    message(STATUS "  Include: ${XMP_INCLUDE_DIRS}")

# ---------------------------------------------------------------------------
# Handle explicit Core/Files library paths
# ---------------------------------------------------------------------------

elseif(XMP_Core_LIB AND XMP_Files_LIB)
    # Use explicitly provided XMP libraries
    set(XMP_FOUND TRUE)
    set(XMP_CORE_LIBRARY ${XMP_Core_LIB})
    set(XMP_FILES_LIBRARY ${XMP_Files_LIB})
    if(XMP_Core_LIB_DEBUG)
        set(XMP_CORE_LIBRARY_DEBUG ${XMP_Core_LIB_DEBUG})
    endif()
    if(XMP_Files_LIB_DEBUG)
        set(XMP_FILES_LIBRARY_DEBUG ${XMP_Files_LIB_DEBUG})
    endif()

    if(XMP_INCLUDE_DIR)
        set(XMP_INCLUDE_DIRS ${XMP_INCLUDE_DIR})
    elseif(XMP_SDK_DIR)
        if(EXISTS "${XMP_SDK_DIR}/public/include")
            set(XMP_INCLUDE_DIRS "${XMP_SDK_DIR}/public/include")
        elseif(EXISTS "${XMP_SDK_DIR}/include")
            set(XMP_INCLUDE_DIRS "${XMP_SDK_DIR}/include")
        endif()
    endif()

    set(XMP_LIBRARIES ${XMP_CORE_LIBRARY} ${XMP_FILES_LIBRARY})

    # Create XMP::Core target
    if(NOT TARGET XMP::Core)
        add_library(XMP::Core UNKNOWN IMPORTED)
        if(XMP_INCLUDE_DIRS)
            set_target_properties(XMP::Core PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIRS}"
            )
        endif()
        if(XMP_CORE_LIBRARY AND XMP_CORE_LIBRARY_DEBUG)
            set_target_properties(XMP::Core PROPERTIES
                IMPORTED_LOCATION_RELEASE "${XMP_CORE_LIBRARY}"
                IMPORTED_LOCATION_DEBUG "${XMP_CORE_LIBRARY_DEBUG}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${XMP_CORE_LIBRARY}"
                IMPORTED_LOCATION_MINSIZEREL "${XMP_CORE_LIBRARY}"
                IMPORTED_LOCATION "${XMP_CORE_LIBRARY}"
            )
        else()
            set_target_properties(XMP::Core PROPERTIES
                IMPORTED_LOCATION "${XMP_CORE_LIBRARY}"
            )
        endif()
    endif()

    # Create XMP::Files target
    if(NOT TARGET XMP::Files)
        add_library(XMP::Files UNKNOWN IMPORTED)
        if(XMP_INCLUDE_DIRS)
            set_target_properties(XMP::Files PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIRS}"
            )
        endif()
        if(XMP_FILES_LIBRARY AND XMP_FILES_LIBRARY_DEBUG)
            set_target_properties(XMP::Files PROPERTIES
                IMPORTED_LOCATION_RELEASE "${XMP_FILES_LIBRARY}"
                IMPORTED_LOCATION_DEBUG "${XMP_FILES_LIBRARY_DEBUG}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${XMP_FILES_LIBRARY}"
                IMPORTED_LOCATION_MINSIZEREL "${XMP_FILES_LIBRARY}"
                IMPORTED_LOCATION "${XMP_FILES_LIBRARY}"
            )
        else()
            set_target_properties(XMP::Files PROPERTIES
                IMPORTED_LOCATION "${XMP_FILES_LIBRARY}"
            )
        endif()
    endif()

    message(STATUS "Found XMP via explicit Core/Files paths")

else()
    # ---------------------------------------------------------------------------
    # Set up search paths for auto-detection
    # ---------------------------------------------------------------------------

    if(XMP_SDK_DIR)
        # User specified XMP SDK directory
        if(XMP_INCLUDE_DIR)
            set(_xmp_include_paths ${XMP_INCLUDE_DIR})
        else()
            set(_xmp_include_paths
                "${XMP_SDK_DIR}/public/include"
                "${XMP_SDK_DIR}/include"
                "${XMP_SDK_DIR}/source/XMPCore/include"
                "${XMP_SDK_DIR}/XMPCore/include"
            )
        endif()

        if(XMP_LIBRARY_DIR)
            set(_xmp_lib_paths ${XMP_LIBRARY_DIR})
        else()
            set(_xmp_lib_paths
                "${XMP_SDK_DIR}/lib"
                "${XMP_SDK_DIR}/libraries"
                "${XMP_SDK_DIR}/build/lib"
                "${XMP_SDK_DIR}/cmake/build/lib"
                "${XMP_SDK_DIR}/public/libraries/windows/x64/Release"
                "${XMP_SDK_DIR}/public/libraries/windows/x64/Debug"
            )
        endif()
    else()
        # Default search paths
        set(_xmp_include_paths
            /usr/include/XMP
            /usr/local/include/XMP
            ${CMAKE_PREFIX_PATH}/include/XMP
            ${CMAKE_PREFIX_PATH}/include
            "C:/Program Files/Adobe/XMP SDK/public/include"
            "C:/Program Files (x86)/Adobe/XMP SDK/public/include"
        )

        set(_xmp_lib_paths
            /usr/lib
            /usr/local/lib
            ${CMAKE_PREFIX_PATH}/lib
            "C:/Program Files/Adobe/XMP SDK/lib"
            "C:/Program Files (x86)/Adobe/XMP SDK/lib"
        )
    endif()

    # ---------------------------------------------------------------------------
    # Search for headers and libraries
    # ---------------------------------------------------------------------------

    find_path(XMP_INCLUDE_DIR
        NAMES XMP.hpp XMPSDK.hpp XMPCore.hpp
        PATHS ${_xmp_include_paths}
        NO_DEFAULT_PATH
    )

    # Try common XMP library names
    find_library(XMP_CORE_LIBRARY
        NAMES XMPCore XMPCoreStatic XMPCoreStaticRelease XMPCoreStaticDebug libXMPCore
        PATHS ${_xmp_lib_paths}
        NO_DEFAULT_PATH
    )

    find_library(XMP_CORE_LIBRARY_DEBUG
        NAMES XMPCoreStaticDebug XMPCored XMPCore_d libXMPCored
        PATHS ${_xmp_lib_paths}
        NO_DEFAULT_PATH
    )

    find_library(XMP_FILES_LIBRARY
        NAMES XMPFiles XMPFilesStatic XMPFilesStaticRelease XMPFilesStaticDebug libXMPFiles
        PATHS ${_xmp_lib_paths}
        NO_DEFAULT_PATH
    )

    find_library(XMP_FILES_LIBRARY_DEBUG
        NAMES XMPFilesStaticDebug XMPFilesd XMPFiles_d libXMPFilesd
        PATHS ${_xmp_lib_paths}
        NO_DEFAULT_PATH
    )

    # ---------------------------------------------------------------------------
    # Set XMP found status and create targets from auto-detection
    # ---------------------------------------------------------------------------

    if(XMP_INCLUDE_DIR AND XMP_CORE_LIBRARY AND XMP_FILES_LIBRARY)
        set(XMP_FOUND TRUE)
        set(XMP_INCLUDE_DIRS ${XMP_INCLUDE_DIR})
        set(XMP_LIBRARIES ${XMP_CORE_LIBRARY} ${XMP_FILES_LIBRARY})

        # Create XMP::Core target
        if(NOT TARGET XMP::Core)
            add_library(XMP::Core UNKNOWN IMPORTED)
            set_target_properties(XMP::Core PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIR}"
            )
            if(XMP_CORE_LIBRARY AND XMP_CORE_LIBRARY_DEBUG)
                set_target_properties(XMP::Core PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${XMP_CORE_LIBRARY}"
                    IMPORTED_LOCATION_DEBUG "${XMP_CORE_LIBRARY_DEBUG}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${XMP_CORE_LIBRARY}"
                    IMPORTED_LOCATION_MINSIZEREL "${XMP_CORE_LIBRARY}"
                    IMPORTED_LOCATION "${XMP_CORE_LIBRARY}"
                )
            else()
                set_target_properties(XMP::Core PROPERTIES
                    IMPORTED_LOCATION "${XMP_CORE_LIBRARY}"
                )
            endif()
        endif()

        # Create XMP::Files target
        if(NOT TARGET XMP::Files)
            add_library(XMP::Files UNKNOWN IMPORTED)
            set_target_properties(XMP::Files PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${XMP_INCLUDE_DIR}"
            )
            if(XMP_FILES_LIBRARY AND XMP_FILES_LIBRARY_DEBUG)
                set_target_properties(XMP::Files PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${XMP_FILES_LIBRARY}"
                    IMPORTED_LOCATION_DEBUG "${XMP_FILES_LIBRARY_DEBUG}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${XMP_FILES_LIBRARY}"
                    IMPORTED_LOCATION_MINSIZEREL "${XMP_FILES_LIBRARY}"
                    IMPORTED_LOCATION "${XMP_FILES_LIBRARY}"
                )
            else()
                set_target_properties(XMP::Files PROPERTIES
                    IMPORTED_LOCATION "${XMP_FILES_LIBRARY}"
                )
            endif()
        endif()

        message(STATUS "Found XMP via auto-detection")
    else()
        set(XMP_FOUND FALSE)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tell CMake whether we succeeded
# ---------------------------------------------------------------------------

find_package_handle_standard_args(XMP
    REQUIRED_VARS XMP_FOUND
)

mark_as_advanced(
    XMP_INCLUDE_DIR
    XMP_CORE_LIBRARY
    XMP_CORE_LIBRARY_DEBUG
    XMP_FILES_LIBRARY
    XMP_FILES_LIBRARY_DEBUG
)
