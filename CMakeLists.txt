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
add_subdirectory( vendor/ImGuizmo )

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
        
        src/Mesh.cpp
        src/Mesh.h
        src/ModelLoader.cpp
        src/ModelLoader.h
        src/Skeleton.h
        src/Animation.h


        src/Viewport.cpp
        src/Viewport.h
        
        src/World.cpp
        src/World.h
        src/Entity.cpp
        src/Entity.h
        
        src/Sky.cpp
        src/Sky.h
        
        vendor/tinyfiledialogs.c
        
        src/AudioSystem.h
        
        src/ScriptEngine.cpp
        src/ScriptEngine.h
        
        src/UID.cpp
        src/UID.h
        
        src/PhysicsCommon.h
)

set(SRC_COMPONENTS
        src/Component.h
        src/AnimatorComponent.cpp
        src/AnimatorComponent.h
        src/AudioComponent.cpp
        src/AudioComponent.h
        src/MeshComponent.cpp
        src/MeshComponent.h
        src/TransformComponent.cpp
        src/TransformComponent.h
        src/ScriptComponent.cpp
        src/ScriptComponent.h
        src/CameraComponent.cpp
        src/CameraComponent.h
        src/MovementComponent.cpp
        src/MovementComponent.h
        src/CollisionComponent.cpp
        src/CollisionComponent.h
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
        src/GizmoRender.cpp
        src/GizmoRender.h
        src/SpriteRenderer.cpp
        src/SpriteRenderer.h
        src/ParticleSystem.cpp
        src/ParticleSystem.h
        src/DecalManager.cpp
        src/DecalManager.h
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
        src/HierarchyPanel.cpp
        src/HierarchyPanel.h
)

set(SRC_Q3
        src/q3/Q3BSPAsset.cpp
        src/q3/Q3BSPAsset.h
        src/q3/Q3BSPTypes.h
        src/q3/Q3BspMesh.cpp
        src/q3/Q3BspMesh.h
        src/q3/Q3BspCollision.cpp
        src/q3/Q3BspCollision.h
        src/q3/Q3Bezier.cpp
        src/q3/Q3Bezier.h
        src/q3/Q3LightGrid.cpp
        src/q3/Q3LightGrid.h
        src/q3/Q3Shaders.cpp
        src/q3/Q3Shaders.h
        src/q3/TextureAtlas.cpp
        src/q3/TextureAtlas.h
)

set(SRC_IMGUI
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/backends/imgui_impl_opengl3.cpp
        #src/imguiStyling.cpp
)

add_executable( ${PROJECT_NAME}
        ${SRC_COMMON}
        ${SRC_Q3}
        ${SRC_COMPONENTS}
        ${SRC_RENDERER}
        ${SRC_PANELS}
        ${SRC_IMGUI}
)

source_group("Common" FILES ${SRC_COMMON})
source_group("Quake 3" FILES ${SRC_Q3})
source_group("Components" FILES ${SRC_COMPONENTS})
source_group("Renderer" FILES ${SRC_RENDERER})
source_group("Panels" FILES ${SRC_PANELS})
source_group("imgui" FILES ${SRC_IMGUI})

target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_XYZW_ONLY)

#set(SHADERS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
#file(GLOB_RECURSE SHADER_FILES "${SHADERS_FOLDER}/*")
#source_group("Shaders" FILES ${SHADER_FILES})
#add_custom_target(Shaders SOURCES ${SHADER_FILES})


# Создаем статическую библиотеку miniaudio
add_library(miniaudio STATIC 
    vendor/miniaudio/miniaudio.c
    vendor/miniaudio/miniaudio.h
)

# Указываем путь к заголовочным файлам
target_include_directories(miniaudio PUBLIC 
    vendor/miniaudio
)

####################### ANGELSCRIPT #######################

# Включаем модуль для скачивания зависимостей
include(FetchContent)

# Объявляем зависимость и СРАЗУ указываем, где лежит правильный CMakeLists.txt внутри репозитория
FetchContent_Declare(
    angelscript
    GIT_REPOSITORY https://github.com/anjo76/angelscript
    GIT_TAG        master
    SOURCE_SUBDIR  sdk/angelscript/projects/cmake
)

# Скачиваем и подключаем подпроект одной командой
FetchContent_MakeAvailable(angelscript)

# 2. Создаем переменные для удобства путей
set(AS_ADDON_DIR "${angelscript_SOURCE_DIR}/sdk/add_on")

# 3. Создаем библиотеку для аддонов и добавляем туда нужные файлы
# Вы можете раскомментировать или добавить другие аддоны по аналогии
add_library(angelscript_addons STATIC
    ${AS_ADDON_DIR}/scriptstdstring/scriptstdstring.cpp
    ${AS_ADDON_DIR}/scriptarray/scriptarray.cpp
    ${AS_ADDON_DIR}/scriptbuilder/scriptbuilder.cpp
    ${AS_ADDON_DIR}/scripthandle/scripthandle.cpp
)

# Указываем, где искать хедеры для самих аддонов
target_include_directories(angelscript_addons PUBLIC ${AS_ADDON_DIR})

# Связываем аддоны с основным AngelScript (чтобы они видели его заголовочные файлы)
target_link_libraries(angelscript_addons PUBLIC angelscript)
target_include_directories(angelscript_addons PUBLIC ${angelscript_SOURCE_DIR}/sdk/angelscript/include)

#######################


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    vendor/glm
    #vendor/ImGuizmo
    ${angelscript_SOURCE_DIR}/sdk/angelscript/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE glad glfw imgui ImGuizmo miniaudio angelscript angelscript_addons)



set(ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_LINK_NAME "${CMAKE_BINARY_DIR}/bin/assets")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/assets
)
