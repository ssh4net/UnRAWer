add_library(dnd_glfw INTERFACE)

target_include_directories(dnd_glfw
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(APPLE)
    add_library(dnd_glfw_macos STATIC
        dnd_glfw_macos.mm
    )

    target_link_libraries(dnd_glfw_macos
        PUBLIC
            "-framework Cocoa"
    )

    target_link_libraries(dnd_glfw
        INTERFACE
            dnd_glfw_macos
    )
endif()
