add_library(dnd_glfw INTERFACE)

target_include_directories(dnd_glfw
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(APPLE)
    add_library(dnd_glfw_macos STATIC
        dnd_glfw_macos.mm
    )

    if(TARGET glfw)
        target_link_libraries(dnd_glfw_macos PRIVATE glfw)
    elseif(TARGET glfw3::glfw)
        target_link_libraries(dnd_glfw_macos PRIVATE glfw3::glfw)
    else()
        find_path(DND_GLFW_GLFW_INCLUDE_DIR
            NAMES GLFW/glfw3.h
            PATHS ${CMAKE_PREFIX_PATH}
            PATH_SUFFIXES include
        )
        if(DND_GLFW_GLFW_INCLUDE_DIR)
            target_include_directories(dnd_glfw_macos PRIVATE ${DND_GLFW_GLFW_INCLUDE_DIR})
        endif()
    endif()

    target_link_libraries(dnd_glfw_macos
        PUBLIC
            "-framework Cocoa"
    )

    target_link_libraries(dnd_glfw
        INTERFACE
            dnd_glfw_macos
    )
endif()
