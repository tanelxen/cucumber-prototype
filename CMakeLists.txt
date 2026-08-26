cmake_minimum_required(VERSION 3.28)
project(Cucumber)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Configure GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# Thirdparty libraries
add_subdirectory( vendor/glad )
add_subdirectory( vendor/glfw )
add_subdirectory( vendor/imgui )

set(SRC_COMMON
        src/main.cpp

        src/Camera.cpp
        src/Camera.h
        src/CameraController.cpp
        src/CameraController.h
        src/Grid.cpp
        src/Grid.h
        
        src/Application.cpp
        src/Application.h
        src/Input.cpp
        src/Input.h
        
        src/DebugRenderer.cpp
        src/DebugRenderer.h
        src/Cube.cpp
        src/Cube.h
        
        src/Mesh.cpp
        src/Mesh.h
        src/ModelLoader.cpp
        src/ModelLoader.h
        src/Skeleton.h
        src/Animation.h
        src/Animator.cpp
        src/Animator.h

        src/Viewport.cpp
        src/Viewport.h
        
        src/World.cpp
        src/World.h
        src/Entity.cpp
        src/Entity.h
        
        vendor/tinyfiledialogs.c
)

set(SRC_RENDERER
        src/RenderDevice.cpp
        src/RenderDevice.h
        src/VertexArray.cpp
        src/VertexArray.h
        src/VertexBuffer.cpp
        src/VertexBuffer.h
        src/VertexLayout.cpp
        src/VertexLayout.h
        src/IndexBuffer.cpp
        src/IndexBuffer.h
        src/Shader.cpp
        src/Shader.h
        src/ShaderVariant.cpp
        src/ShaderVariant.h
)

set(SRC_PANELS
        src/UIManager.cpp
        src/UIManager.h

        src/ViewportPanel.cpp
        src/ViewportPanel.h
        src/ScenePanel.cpp
        src/ScenePanel.h
        src/InspectorPanel.cpp
        src/InspectorPanel.h
        src/AssetsPanel.cpp
        src/AssetsPanel.h
        src/MaterialPanel.cpp
        src/MaterialPanel.h
)

set(SRC_IMGUI
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/backends/imgui_impl_opengl3.cpp
        #src/imguiStyling.cpp
)

add_executable( ${PROJECT_NAME}
        ${SRC_COMMON}
        ${SRC_RENDERER}
        ${SRC_PANELS}
        ${SRC_IMGUI}
)

source_group("Common" FILES ${SRC_COMMON})
source_group("Renderer" FILES ${SRC_RENDERER})
source_group("Panels" FILES ${SRC_PANELS})
source_group("imgui" FILES ${SRC_IMGUI})

target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_XYZW_ONLY)

#set(SHADERS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
#file(GLOB_RECURSE SHADER_FILES "${SHADERS_FOLDER}/*")
#source_group("Shaders" FILES ${SHADER_FILES})
#add_custom_target(Shaders SOURCES ${SHADER_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE vendor/glm)

target_link_libraries(${PROJECT_NAME} PRIVATE glad glfw imgui)



set(ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_LINK_NAME "${CMAKE_BINARY_DIR}/bin/assets")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/assets
)
