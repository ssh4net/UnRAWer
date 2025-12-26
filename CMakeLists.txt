cmake_minimum_required(VERSION 3.20)

# Add custom module paths for Find modules (cmake/ and cmake/modules/)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
)

# ------------------------------------------------------------------------------
#  Project metadata
# ------------------------------------------------------------------------------
project(UnRAWer LANGUAGES CXX C VERSION 0.1.0)

# ------------------------------------------------------------------------------
#  Configuration restrictions (Debug and Release only)
# ------------------------------------------------------------------------------
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Available build configurations" FORCE)

# ------------------------------------------------------------------------------
#  User options
# ------------------------------------------------------------------------------
option(UNRAWER_USE_PCH "Use pre-compiled headers" ON)
option(UNRAWER_STATIC_LINK "Link against static third-party libraries (adds required preprocessor definitions)" ON)
option(UNRAWER_WITH_DNG "Enable DNG/XMP support (requires Adobe DNG SDK and XMP SDK)" ON)
set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Postfix for Debug configuration")
set(UNRAWER_TOML11_DIR "" CACHE PATH "Path to toml11 headers if not provided by your toolchain")

# Adobe SDK paths (only used if UNRAWER_WITH_DNG is ON)
set(XMP_SDK_DIR "" CACHE PATH "Path to Adobe XMP SDK root directory")
set(DNG_SDK_DIR "" CACHE PATH "Path to Adobe DNG SDK root directory")
set(OPENJPH_LIBRARIES "" CACHE STRING "OpenJPH library paths (semicolon separated for Release;Debug)")
set(OPENJPH_INCLUDE_DIR "" CACHE PATH "Path to OpenJPH include directory")

# ------------------------------------------------------------------------------
#  Toolchain / policies
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set MSVC runtime library globally (must be set before finding dependencies)
# This controls which x265 library variant to link on Windows:
#   - MultiThreaded = /MT (static runtime) -> uses x265-static.lib
#   - MultiThreadedDLL = /MD (dynamic runtime) -> uses libx265.lib
if(MSVC)
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        # Default to static runtime for consistent deployment
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# ------------------------------------------------------------------------------
#  GUI and Windowing Dependencies
# ------------------------------------------------------------------------------
find_package(glfw3 3.3 REQUIRED)

if(TARGET glfw3::glfw3)
    message(STATUS "Found target: glfw3::glfw3")
elseif(TARGET glfw)
    # Common on Linux - the target is just 'glfw'
    message(STATUS "Found target: glfw -> Aliasing to glfw3::glfw3")
    add_library(glfw3::glfw3 ALIAS glfw)
elseif(TARGET glfw3)
    message(STATUS "Found target: glfw3 -> Aliasing to glfw3::glfw3")
    add_library(glfw3::glfw3 ALIAS glfw3)
elseif(TARGET GLFW3::glfw3)
    message(STATUS "Found target: GLFW3::glfw3 -> Aliasing to glfw3::glfw3")
    add_library(glfw3::glfw3 ALIAS GLFW3::glfw3)
else()
    message(STATUS "No GLFW targets found. Checking variables...")
    # Attempt to find libraries manually if variables are empty but config was found
    if (NOT GLFW3_LIBRARIES AND NOT glfw3_LIBRARIES)
         # Fallback for some windows configs
         set(GLFW3_LIBRARIES "${glfw3_DIR}/../../glfw3.lib")
         if (NOT EXISTS ${GLFW3_LIBRARIES})
            set(GLFW3_LIBRARIES "")
         endif()
    endif()

    if(GLFW3_LIBRARIES)
        message(STATUS "Creating glfw3::glfw3 from GLFW3_LIBRARIES: ${GLFW3_LIBRARIES}")
        add_library(glfw3::glfw3 UNKNOWN IMPORTED)
        set_target_properties(glfw3::glfw3 PROPERTIES
            IMPORTED_LOCATION "${GLFW3_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW3_INCLUDE_DIR}"
        )
    elseif(glfw3_LIBRARIES)
         message(STATUS "Creating glfw3::glfw3 from glfw3_LIBRARIES: ${glfw3_LIBRARIES}")
         add_library(glfw3::glfw3 UNKNOWN IMPORTED)
         set_target_properties(glfw3::glfw3 PROPERTIES
            IMPORTED_LOCATION "${glfw3_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${glfw3_INCLUDE_DIR}"
         )
    else()
        message(FATAL_ERROR "GLFW3 found but no targets or libraries variables set!")
    endif()
endif()
find_package(OpenGL REQUIRED)

# Platform-specific dependencies
if(UNIX AND NOT APPLE)
    # X11 libraries (required by GLFW on Linux)
    find_package(X11 REQUIRED)

    # OpenMP (LibRaw may be built with OpenMP support)
    find_package(OpenMP QUIET)
    if(NOT OpenMP_CXX_FOUND)
        # Try to find libomp directly (for clang)
        find_library(LIBOMP_LIBRARY NAMES omp libomp HINTS /usr/lib /usr/lib/llvm-20/lib)
        if(LIBOMP_LIBRARY)
            set(OpenMP_FOUND TRUE)
        endif()
    else()
        set(OpenMP_FOUND TRUE)
    endif()
endif()

# Find ImGui header as a proxy for the library location
find_path(IMGUI_INCLUDE_DIR "imgui.h"
    HINTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui"
    DOC "Path to ImGui source directory"
)

# Find dnd_glfw header
find_path(DND_GLFW_INCLUDE_DIR "dnd_glfw.h"
    HINTS "${CMAKE_CURRENT_SOURCE_DIR}/external/dnd_glfw"
    DOC "Path to dnd_glfw header directory"
)

if(NOT IMGUI_INCLUDE_DIR)
    message(FATAL_ERROR "ImGui not found. Please set IMGUI_INCLUDE_DIR to the root of the ImGui source directory.")
endif()
if(NOT DND_GLFW_INCLUDE_DIR)
    message(FATAL_ERROR "dnd_glfw.h not found. Please set DND_GLFW_INCLUDE_DIR to its location.")
endif()

message(STATUS "Found ImGui: ${IMGUI_INCLUDE_DIR}")
message(STATUS "Found dnd_glfw: ${DND_GLFW_INCLUDE_DIR}")

# Find toml11 (header-only library for TOML config parsing)
if(NOT UNRAWER_TOML11_DIR)
    # Try to find toml.hpp in standard locations
    find_path(TOML11_INCLUDE_DIR
        NAMES toml.hpp toml11/toml.hpp
        HINTS
            ${CMAKE_PREFIX_PATH}/include
            /usr/include
            /usr/local/include
        DOC "Path to toml11 headers"
    )
    if(TOML11_INCLUDE_DIR)
        set(UNRAWER_TOML11_DIR ${TOML11_INCLUDE_DIR})
    endif()
endif()

if(UNRAWER_TOML11_DIR)
    set(TOML11_FOUND TRUE)
    message(STATUS "Found toml11: ${UNRAWER_TOML11_DIR}")
else()
    set(TOML11_FOUND FALSE)
    message(FATAL_ERROR "toml11 not found. Please set UNRAWER_TOML11_DIR to the toml11 include directory.")
endif()

# ------------------------------------------------------------------------------
#  Third-party dependencies
# ------------------------------------------------------------------------------

# OpenJPH support - Find this BEFORE OpenEXR as static OpenEXR builds often depend on it
# OpenEXR's config uses find_dependency(openjph) which needs openjph_FOUND to be TRUE
# If user provides OPENJPH_LIBRARIES, use our Find module which handles manual overrides
# Otherwise try Config mode first, then fall back to our Find module
if(OPENJPH_LIBRARIES)
    # Manual override - use our Find module directly (sets openjph_FOUND)
    find_package(OpenJPH QUIET)
elseif(NOT openjph_FOUND AND NOT TARGET openjph)
    # Try Config mode first
    find_package(openjph CONFIG QUIET)
    if(NOT openjph_FOUND AND NOT TARGET openjph)
        # Fall back to our Find module
        find_package(OpenJPH QUIET)
    endif()
endif()

# CRITICAL: Set openjph_FOUND BEFORE OpenEXR so find_dependency(openjph) succeeds
# OpenEXR's config calls find_dependency(openjph) which will fail without this
if(TARGET openjph OR OPENJPH_FOUND)
    set(openjph_FOUND TRUE CACHE BOOL "OpenJPH found" FORCE)
    set(openjph_LIBRARIES "${OPENJPH_LIBRARIES}" CACHE STRING "OpenJPH libraries" FORCE)
    set(openjph_INCLUDE_DIRS "${OPENJPH_INCLUDES}" CACHE STRING "OpenJPH includes" FORCE)
    message(STATUS "OpenJPH pre-configured for OpenEXR dependency: openjph_FOUND=${openjph_FOUND}")
endif()

# Find Imath and OpenEXR first, as OpenImageIO depends on them
find_package(Imath CONFIG)
find_package(OpenEXR CONFIG REQUIRED)

# Find GIF (dependency of OpenImageIO)
find_package(GIF QUIET)
if(NOT TARGET GIF::GIF)
    find_package(GIFLIB CONFIG QUIET)
    if(TARGET GIFLIB::GIFLIB)
        add_library(GIF::GIF ALIAS GIFLIB::GIFLIB)
    endif()
endif()

# Find Ptex (dependency of OpenImageIO)
find_package(Ptex CONFIG QUIET)
if(NOT TARGET Ptex::Ptex_static)
    if(TARGET Ptex::Ptex)
         add_library(Ptex::Ptex_static ALIAS Ptex::Ptex)
    elseif(TARGET Ptex)
         add_library(Ptex::Ptex_static ALIAS Ptex)
    else()
        # Fallback to module
        find_package(Ptex QUIET)
        if(TARGET Ptex::Ptex_dynamic)
             add_library(Ptex::Ptex_static ALIAS Ptex::Ptex_dynamic)
        endif()
    endif()
endif()

# Find WebP (dependency of OpenImageIO)
find_package(WebP QUIET)
if(NOT TARGET WebP::webp)
    find_package(webp CONFIG QUIET)
    if(TARGET webp::webp)
        add_library(WebP::webp ALIAS webp::webp)
    elseif(TARGET WebP::WebP)
        add_library(WebP::webp ALIAS WebP::WebP)
    endif()
endif()

# Find OpenJPEG (dependency of OpenImageIO)
find_package(OpenJPEG CONFIG QUIET)
if(NOT TARGET openjp2)
    # If Config didn't define 'openjp2', try Module
    find_package(OpenJPEG QUIET)
    if(TARGET OpenJPEG::OpenJPEG)
        add_library(openjp2 ALIAS OpenJPEG::OpenJPEG)
    elseif(TARGET openjpeg)
        add_library(openjp2 ALIAS openjpeg)
    endif()
endif()

# Pre-find PNG and ZLIB before OpenImageIO (OIIO may require specific target names)
find_package(ZLIB QUIET)
find_package(PNG QUIET)

# Create PNG::png_static alias if needed (OpenImageIO may require this target name)
if(TARGET PNG::PNG AND NOT TARGET PNG::png_static)
    add_library(PNG::png_static ALIAS PNG::PNG)
endif()

# Find OpenImageIO and handle legacy target names
find_package(OpenImageIO REQUIRED)

if(NOT TARGET OpenImageIO::OpenImageIO)
    if(TARGET OpenImageIO)
        # Alias legacy target name to namespaced target
        add_library(OpenImageIO::OpenImageIO ALIAS OpenImageIO)
    elseif(OPENIMAGEIO_LIBRARIES)
        # Create imported target from variables if no target exists
        add_library(OpenImageIO::OpenImageIO UNKNOWN IMPORTED)
        set_target_properties(OpenImageIO::OpenImageIO PROPERTIES
            IMPORTED_LOCATION "${OPENIMAGEIO_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENIMAGEIO_INCLUDE_DIRS}"
        )
    endif()
endif()

# Find LibRaw using OpenImageIO's proper Find module
find_package(LibRaw REQUIRED)
find_package(spdlog REQUIRED)

# Manual search for static dependencies missing from OIIO/LibRaw config interface
set(EXTRA_STATIC_LIBS "")
macro(add_static_dep NAME_REL NAME_DBG)
    find_library(LIB_${NAME_REL}_RELEASE NAMES ${NAME_REL} HINTS "${CMAKE_PREFIX_PATH}/lib")
    find_library(LIB_${NAME_REL}_DEBUG NAMES ${NAME_DBG} HINTS "${CMAKE_PREFIX_PATH}/lib")
    if(LIB_${NAME_REL}_RELEASE)
        list(APPEND EXTRA_STATIC_LIBS "$<$<CONFIG:Release>:${LIB_${NAME_REL}_RELEASE}>")
    endif()
    if(LIB_${NAME_REL}_DEBUG)
        list(APPEND EXTRA_STATIC_LIBS "$<$<CONFIG:Debug>:${LIB_${NAME_REL}_DEBUG}>")
    endif()
endmacro()

add_static_dep(zstd_static zstd_staticd)
add_static_dep(lzma lzmad)
add_static_dep(freetype freetyped)
add_static_dep(glib-2.0 glib-2.0d)  # Required by harfbuzz
add_static_dep(harfbuzz harfbuzzd)
add_static_dep(harfbuzz-subset harfbuzz-subsetd)
add_static_dep(heif heifd)
add_static_dep(hwy hwyd)
add_static_dep(libde265 libde265d)
add_static_dep(de265 de265d)  # Alternative name

# Find x265 using custom module that selects correct library based on runtime
find_package(X265 QUIET)
if(X265_FOUND)
    list(APPEND EXTRA_STATIC_LIBS X265::X265)
endif()

add_static_dep(libkvazaar libkvazaard)
add_static_dep(kvazaar kvazaard)  # Alternative name
add_static_dep(aom aomd)
add_static_dep(drm drmd)  # FFmpeg DRM support on Linux
add_static_dep(bz2 bz2d)
add_static_dep(brotlicommon brotlicommond)
add_static_dep(brotlidec brotlidecd)
add_static_dep(brotlienc brotliencd)

# LCMS2 - Try multiple detection methods in order:
# 1. CMake find_package (Config mode)
# 2. pkg-config
# 3. User-provided LCMS2_LIBRARIES/LCMS2_INCLUDE_DIR
# 4. Manual search

# Method 1: Try CMake Config mode
find_package(lcms2 CONFIG QUIET)
if(lcms2_FOUND OR LCMS2_FOUND)
    set(LCMS2_FOUND TRUE)
    if(TARGET lcms2::lcms2)
        list(APPEND EXTRA_STATIC_LIBS lcms2::lcms2)
        message(STATUS "Found LCMS2 via CMake Config: lcms2::lcms2")
    endif()
endif()

# Method 2: Try pkg-config
if(NOT LCMS2_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LCMS2_PC QUIET lcms2)
        if(LCMS2_PC_FOUND)
            set(LCMS2_FOUND TRUE)
            set(LCMS2_INCLUDE_DIR ${LCMS2_PC_INCLUDE_DIRS})
            # Find the actual library file
            find_library(LCMS2_LIBRARY
                NAMES lcms2 liblcms2
                HINTS ${LCMS2_PC_LIBRARY_DIRS} ${CMAKE_PREFIX_PATH}/lib
            )
            if(LCMS2_LIBRARY)
                list(APPEND EXTRA_STATIC_LIBS ${LCMS2_LIBRARY})
                message(STATUS "Found LCMS2 via pkg-config: ${LCMS2_LIBRARY}")
            else()
                list(APPEND EXTRA_STATIC_LIBS ${LCMS2_PC_LIBRARIES})
                message(STATUS "Found LCMS2 via pkg-config: ${LCMS2_PC_LIBRARIES}")
            endif()
        endif()
    endif()
endif()

# Method 3: User-provided explicit paths
if(NOT LCMS2_FOUND AND LCMS2_LIBRARIES)
    set(LCMS2_FOUND TRUE)
    foreach(LIB ${LCMS2_LIBRARIES})
        string(TOLOWER "${LIB}" LIB_LOWER)
        if(LIB_LOWER MATCHES "d\\.lib$" OR LIB_LOWER MATCHES "d\\.a$")
            list(APPEND EXTRA_STATIC_LIBS "$<$<CONFIG:Debug>:${LIB}>")
        else()
            list(APPEND EXTRA_STATIC_LIBS "$<$<CONFIG:Release>:${LIB}>")
        endif()
    endforeach()
    message(STATUS "Found LCMS2 via user-provided paths: ${LCMS2_LIBRARIES}")
endif()

# Method 4: Manual search fallback
if(NOT LCMS2_FOUND)
    find_path(LCMS2_INCLUDE_DIR NAMES lcms2.h
        HINTS ${CMAKE_PREFIX_PATH}/include
    )
    find_library(LCMS2_LIBRARY
        NAMES lcms2 liblcms2 lcms2_static
        HINTS ${CMAKE_PREFIX_PATH}/lib
    )
    if(LCMS2_INCLUDE_DIR AND LCMS2_LIBRARY)
        set(LCMS2_FOUND TRUE)
        list(APPEND EXTRA_STATIC_LIBS ${LCMS2_LIBRARY})
        message(STATUS "Found LCMS2 via manual search: ${LCMS2_LIBRARY}")
    else()
        # Last resort: try Windows static lib names via add_static_dep
        add_static_dep(lcms2_static lcms2_staticd)
    endif()
endif()

add_static_dep(libsharpyuv libsharpyuvd)
add_static_dep(libexpatMT libexpatdMT)
add_static_dep(minizip-ng minizip-ngd)
add_static_dep(pystring pystringd)
add_static_dep(pugixml pugixmld)
add_static_dep(yaml-cpp yaml-cppd)
add_static_dep(deflatestatic deflatestaticd)

# FFmpeg and others
add_static_dep(avcodec avcodecd)
add_static_dep(avdevice avdeviced)
add_static_dep(avfilter avfilterd)
add_static_dep(avformat avformatd)
add_static_dep(avutil avutild)
add_static_dep(swresample swresampled)
add_static_dep(swscale swscaled)
add_static_dep(jasper jasperd)

# JXL
add_static_dep(jxl jxld)
add_static_dep(jxl_threads jxl_threadsd)
add_static_dep(jxl_cms jxl_cmsd)

# WebP extras
add_static_dep(libwebpdecoder libwebpdecoderd)
add_static_dep(libwebpdemux libwebpdemuxd)
add_static_dep(libwebpmux libwebpmuxd)

# OCIO
add_static_dep(OpenColorIO OpenColorIO_d)

# XMP and DNG SDK (optional, for DNG file support)
if(UNRAWER_WITH_DNG)
    find_package(XMP QUIET)
    if(XMP_FOUND)
        if(TARGET XMP::Core)
            list(APPEND EXTRA_STATIC_LIBS XMP::Core XMP::Files)
        elseif(TARGET XMP::XMP)
            list(APPEND EXTRA_STATIC_LIBS XMP::XMP)
        endif()
    else()
        message(WARNING "XMP SDK not found. DNG support may be limited.")
    endif()

    find_package(DNG QUIET)
    if(DNG_FOUND)
        if(TARGET DNG::SDK)
            list(APPEND EXTRA_STATIC_LIBS DNG::SDK)
        endif()
    else()
        message(WARNING "DNG SDK not found. DNG support will be disabled.")
        set(UNRAWER_WITH_DNG OFF)
    endif()
else()
    message(STATUS "DNG/XMP support disabled (UNRAWER_WITH_DNG=OFF)")
    set(XMP_FOUND FALSE)
    set(DNG_FOUND FALSE)
endif()

# OpenJPH (needed by OpenEXR, libheif, OpenImageIO for HTJ2K support)
# Must be linked after the libraries that depend on it
if(TARGET openjph)
    list(APPEND EXTRA_STATIC_LIBS openjph)
elseif(openjph_FOUND)
    # Find the library file if not already a target
    find_library(OPENJPH_LIBRARY
        NAMES openjph libopenjph
        HINTS ${CMAKE_PREFIX_PATH}/lib
    )
    if(OPENJPH_LIBRARY)
        list(APPEND EXTRA_STATIC_LIBS ${OPENJPH_LIBRARY})
    endif()
endif()

# Misc
add_static_dep(libglew32 libglew32d)
add_static_dep(uhdr uhdrd)
add_static_dep(libpng18_static libpng18_staticd)
add_static_dep(zlibstatic zlibstaticd)
add_static_dep(OpenImageIO_Util OpenImageIO_Util_d)

# Pre-find TIFF for OpenImageIO dependencies
find_package(TIFF QUIET)

# Find JPEG manually to ensure jpeg-static.lib is used
find_path(JPEG_INCLUDE_DIR NAMES jpeglib.h)
find_library(JPEG_LIBRARY_RELEASE NAMES jpeg-static jpeg libjpeg)
find_library(JPEG_LIBRARY_DEBUG NAMES jpeg-staticd jpegd libjpegd)

if(JPEG_LIBRARY_RELEASE OR JPEG_LIBRARY_DEBUG)
    if(NOT TARGET JPEG::JPEG)
        add_library(JPEG::JPEG UNKNOWN IMPORTED)
        set_target_properties(JPEG::JPEG PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}")
        
        if(JPEG_LIBRARY_RELEASE)
            set_property(TARGET JPEG::JPEG APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION_RELEASE "${JPEG_LIBRARY_RELEASE}")
        endif()
        
        if(JPEG_LIBRARY_DEBUG)
            set_property(TARGET JPEG::JPEG APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(JPEG::JPEG PROPERTIES IMPORTED_LOCATION_DEBUG "${JPEG_LIBRARY_DEBUG}")
        endif()
    endif()
endif()

# Fallback if manual finding failed
if(NOT TARGET JPEG::JPEG)
    find_package(JPEG QUIET)
endif()


# ------------------------------------------------------------------------------
#  Dependency Status Summary
# ------------------------------------------------------------------------------
# Helper function to display found/not-found status
function(dependency_status NAME FOUND_VAR)
    if(${FOUND_VAR})
        message(STATUS "  ${NAME}: Found")
    else()
        message(STATUS "  ${NAME}: NOT FOUND")
    endif()
endfunction()

message(STATUS "")
message(STATUS "===============================================")
message(STATUS "  UnRAWer Dependency Detection Summary")
message(STATUS "===============================================")
message(STATUS "")
message(STATUS "Core Dependencies (Required):")
dependency_status("GLFW3" glfw3_FOUND)
dependency_status("OpenGL" OPENGL_FOUND)
dependency_status("ImGui" IMGUI_INCLUDE_DIR)
dependency_status("dnd_glfw" DND_GLFW_INCLUDE_DIR)
dependency_status("toml11" TOML11_FOUND)
dependency_status("OpenImageIO" OpenImageIO_FOUND)
dependency_status("LibRaw" LibRaw_FOUND)
dependency_status("spdlog" spdlog_FOUND)
dependency_status("OpenEXR" OpenEXR_FOUND)
dependency_status("Imath" Imath_FOUND)
message(STATUS "")
message(STATUS "Optional Dependencies:")
dependency_status("OpenColorIO" OpenColorIO_FOUND)
dependency_status("LCMS2" LCMS2_FOUND)
dependency_status("OpenJPH (HTJ2K)" openjph_FOUND)
message(STATUS "")
message(STATUS "DNG Support (UNRAWER_WITH_DNG=${UNRAWER_WITH_DNG}):")
dependency_status("DNG SDK" DNG_FOUND)
dependency_status("XMP SDK" XMP_FOUND)
message(STATUS "")
message(STATUS "Image Format Support (via OIIO):")
dependency_status("JPEG" JPEG_FOUND)
dependency_status("PNG" PNG_FOUND)
dependency_status("TIFF" TIFF_FOUND)
dependency_status("ZLIB" ZLIB_FOUND)
dependency_status("WebP" WebP_FOUND)
dependency_status("OpenJPEG" OpenJPEG_FOUND)
dependency_status("GIF" GIF_FOUND)
if(UNIX AND NOT APPLE)
    message(STATUS "")
    message(STATUS "Linux Platform Dependencies:")
    dependency_status("X11" X11_FOUND)
    dependency_status("OpenMP" OpenMP_FOUND)
endif()
message(STATUS "")
message(STATUS "===============================================")


# ------------------------------------------------------------------------------
#  Sources
# ------------------------------------------------------------------------------

# Add ImGui sources using the found path
file(GLOB IMGUI_SOURCES
    "${IMGUI_INCLUDE_DIR}/imgui.cpp"
    "${IMGUI_INCLUDE_DIR}/imgui_draw.cpp"
    "${IMGUI_INCLUDE_DIR}/imgui_tables.cpp"
    "${IMGUI_INCLUDE_DIR}/imgui_widgets.cpp"
    "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_opengl3.cpp"
)

file(GLOB_RECURSE UNRAWER_SOURCES CONFIGURE_DEPENDS
     UnRAWer/src/*.cpp
     UnRAWer/src/*.h
)

# ------------------------------------------------------------------------------
#  Target definition
# ------------------------------------------------------------------------------
add_executable(UnRAWer ${UNRAWER_SOURCES} ${IMGUI_SOURCES})
set_target_properties(UnRAWer PROPERTIES DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")

# Tell the compiler where to find project and dependency headers
target_include_directories(UnRAWer PRIVATE
    UnRAWer/src
    ${DND_GLFW_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    "${IMGUI_INCLUDE_DIR}/backends"
    ${OPENGL_INCLUDE_DIR}
    ${LibRaw_INCLUDE_DIR}
    ${LCMS2_INCLUDE_DIR}
)

if(UNRAWER_TOML11_DIR)
    target_include_directories(UnRAWer PRIVATE "${UNRAWER_TOML11_DIR}")
endif()

# ... (PCH configuration can remain, but must be checked for compatibility)

# ------------------------------------------------------------------------------
#  Linking
# ------------------------------------------------------------------------------

# Core libraries
target_link_libraries(UnRAWer PRIVATE
    OpenImageIO::OpenImageIO
    spdlog::spdlog
    glfw3::glfw3
    LibRaw::LibRaw
    ${OPENGL_LIBRARIES}
    ${EXTRA_STATIC_LIBS}
)

# OpenJPH must be linked at the end to resolve symbols needed by OpenEXR/libheif/OIIO
# Static library link order matters - symbols providers must come after consumers
# IMPORTANT: We bypass the openjph target and link the library file directly because
# the target may not have IMPORTED_LOCATION set correctly for all configurations
if(openjph_FOUND)
    # Find library file directly - don't use the target
    find_library(OPENJPH_LINK_LIBRARY NAMES openjph libopenjph
        HINTS ${CMAKE_PREFIX_PATH}/lib /mnt/e/UBS/lib
        NO_DEFAULT_PATH
    )
    if(OPENJPH_LINK_LIBRARY)
        target_link_libraries(UnRAWer PRIVATE ${OPENJPH_LINK_LIBRARY})
        message(STATUS "Linking OpenJPH library at end: ${OPENJPH_LINK_LIBRARY}")
    else()
        message(WARNING "OpenJPH library file not found despite openjph_FOUND=TRUE")
    endif()
else()
    message(WARNING "OpenJPH not found - HTJ2K support will be unavailable (OpenEXR/libheif/OIIO may fail)")
endif()

# Platform-specific system libraries
if(WIN32)
    target_link_libraries(UnRAWer PRIVATE
        advapi32 Authz comdlg32 crypt32 d3d11 d3d12 D3d9 Dwmapi dxgi dxguid
        gdi32 Imm32 kernel32 mfplat mfuuid mpr ncrypt Ole32 ole32 Oleaut32
        psapi RuntimeObject secur32 Setupapi Shcore shell32 Shlwapi strmiids
        user32 userenv Uuid UxTheme Version WindowsApp windowscodecs Winmm
        winspool Ws2_32 Wtsapi32
    )
elseif(UNIX AND NOT APPLE)
    # Linux system libraries
    target_link_libraries(UnRAWer PRIVATE
        pthread
        dl
        m
        ${X11_LIBRARIES}
        ${X11_Xrandr_LIB}
        ${X11_Xinerama_LIB}
        ${X11_Xcursor_LIB}
        ${X11_Xi_LIB}
    )

    # Link OpenMP if available
    if(OpenMP_CXX_FOUND)
        target_link_libraries(UnRAWer PRIVATE OpenMP::OpenMP_CXX)
    elseif(LIBOMP_LIBRARY)
        target_link_libraries(UnRAWer PRIVATE ${LIBOMP_LIBRARY})
    endif()
elseif(APPLE)
    # macOS frameworks
    target_link_libraries(UnRAWer PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

if(MSVC)
    # Apply the globally configured MSVC runtime library to the target
    # Runtime library is set earlier (line ~48) and controls dependency linking
    set_property(TARGET UnRAWer PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")

    # Additional compiler options to match Visual Studio project
    target_compile_options(UnRAWer PRIVATE
        "/permissive-"
        "/Zc:__cplusplus"
        "/std:c++20"
        "/utf-8"
    )
endif()

# Define preprocessor macros for static linkage
target_compile_definitions(UnRAWer PRIVATE
    SPDLOG_USE_STD_FORMAT
    JXL_STATIC_DEFINE=1
    OPJ_STATIC
    LIBDE265_STATIC_BUILD
    KVZ_STATIC_LIB
    LIBHEIF_STATIC_BUILD
    LIBRAW_NODLL
    OIIO_STATIC_DEFINE=1
    PCRE2_STATIC
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(UnRAWer PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# DNG support definition
if(UNRAWER_WITH_DNG AND DNG_FOUND)
    target_compile_definitions(UnRAWer PRIVATE UNRAWER_WITH_DNG=1)
endif()

# Copy runtime configuration next to the built executable for convenience
add_custom_command(TARGET UnRAWer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/UnRAWer/src/unrw_config.toml"
            $<TARGET_FILE_DIR:UnRAWer>/unrw_config.toml)

# Copy FiraSans-Regular.otf font to fonts directory
add_custom_command(TARGET UnRAWer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:UnRAWer>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/external/Fira/FiraSans-Regular.otf"
            $<TARGET_FILE_DIR:UnRAWer>/fonts/FiraSans-Regular.otf)

# ------------------------------------------------------------------------------
#  Installation
# ------------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS UnRAWer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES UnRAWer/src/unrw_config.toml DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES external/Fira/FiraSans-Regular.otf DESTINATION ${CMAKE_INSTALL_BINDIR}/fonts)

 