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
        
        src/Application.cpp
        src/Application.h
        src/ProjectManager.cpp
        src/ProjectManager.h
        src/Input.cpp
        src/Input.h

        src/ModelLoader.cpp
        src/ModelLoader.h
        src/Skeleton.h
        src/Animation.h
        
        vendor/tinyfiledialogs.c
        
        src/AudioSystem.h
        
        src/ScriptEngine.cpp
        src/ScriptEngine.h
        src/AngelScriptBackend.cpp
        src/AngelScriptRuntime.cpp
        src/AngelScriptRuntime.h
        src/ScriptRuntime.h
        
        src/UID.cpp
        src/UID.h
        
        src/PhysicsCommon.h
        
        src/PhysicsSystem.cpp
        src/PhysicsSystem.h
        src/PhysicsContactListener.cpp
        src/PhysicsContactListener.h
        
        src/TextureManager.cpp
        src/TextureManager.h
        
        src/Bounds.h
        src/SpatialGrid.h
)

set(SRC_SCENE
        src/World.cpp
        src/World.h
        src/Entity.cpp
        src/Entity.h
        src/ExposedProperty.h
        src/SceneSerializer.cpp
        src/SceneSerializer.h
        src/SceneManager.cpp
        src/SceneManager.h
        src/CameraManager.cpp
        src/CameraManager.h
        src/SceneExtractor.cpp
        src/SceneExtractor.h
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
        src/MapComponent.cpp
        src/MapComponent.h
        src/RigidbodyComponent.cpp
        src/RigidbodyComponent.h
        src/LightComponent.cpp
        src/LightComponent.h
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
        src/RenderTarget.cpp
        src/RenderTarget.h
        src/Texture.cpp
        src/Texture.h
        src/Mesh.cpp
        src/Mesh.h
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
        
        src/Renderer.cpp
        src/Renderer.h
        src/IViewProvider.h
        src/GameViewProducer.cpp
        src/GameViewProducer.h
        src/ViewDesc.h
        src/RenderScene.cpp
        src/RenderScene.h
        src/ViewBuilder.cpp
        src/ViewBuilder.h
        src/ShadowSystem.cpp
        src/ShadowSystem.h
        src/ShadowTypes.h
)

set(SRC_GameUI
        src/GameUI.cpp
        src/GameUI.h
        src/GameUIScreen.cpp
        src/GameUIScreen.h
        src/GameUI_AS.cpp
        src/GameUI_AS.h
)

set(SRC_PANELS
        src/UIManager.cpp
        src/UIManager.h

        src/ViewportPanel.cpp
        src/ViewportPanel.h
        src/InspectorPanel.cpp
        src/InspectorPanel.h
        src/ComponentInspector.cpp
        src/ComponentInspector.h
        src/AssetsPanel.cpp
        src/AssetsPanel.h
        src/HierarchyPanel.cpp
        src/HierarchyPanel.h
        
        src/EditorCamera.cpp
        src/EditorCamera.h
        src/EditorViewport.cpp
        src/EditorViewport.h
)

set(SRC_TRENCHBROOM
        src/trenchbroom/Q3Shaders.cpp
        src/trenchbroom/Q3Shaders.h
        src/trenchbroom/MapAsset.cpp
        src/trenchbroom/MapAsset.h
        src/trenchbroom/MapMesh.cpp
        src/trenchbroom/MapMesh.h

        src/trenchbroom/LightmapBaker.cpp
        src/trenchbroom/LightmapBaker.h
        src/trenchbroom/LightmapBakeService.cpp
        src/trenchbroom/LightmapBakeService.h
        src/trenchbroom/LightmapCache.cpp
        src/trenchbroom/LightmapCache.h
        src/trenchbroom/LightmapController.cpp
        src/trenchbroom/LightmapController.h
        src/trenchbroom/Parallel.h
        src/trenchbroom/Half.h

        src/trenchbroom/LevelCooker.cpp
        src/trenchbroom/LevelCooker.h
        src/trenchbroom/MapImporter.cpp
        src/trenchbroom/MapImporter.h
        src/trenchbroom/MapMeshBuilder.h
)

set(SRC_GEOMETRY
    src/geometry/Epsilon.h
    src/geometry/Plane.h
    src/geometry/Polygon.cpp
    src/geometry/Polygon.h
    src/geometry/Brush.cpp
    src/geometry/Brush.h
)

set(SRC_LEVEL
    src/level/LevelDocument.cpp
    src/level/LevelDocument.h
    src/level/LevelDocumentIO.cpp
    src/level/LevelDocumentIO.h
)

set(SRC_IMGUI
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/backends/imgui_impl_opengl3.cpp
        #src/imguiStyling.cpp
)

add_executable( ${PROJECT_NAME}
        ${SRC_COMMON}
        ${SRC_SCENE}
        ${SRC_COMPONENTS}
        ${SRC_RENDERER}
        ${SRC_PANELS}
        ${SRC_GameUI}
        ${SRC_TRENCHBROOM}
        ${SRC_GEOMETRY}
        ${SRC_LEVEL}
        ${SRC_IMGUI}
)

source_group("Common" FILES ${SRC_COMMON})
source_group("Scene" FILES ${SRC_SCENE})
source_group("Components" FILES ${SRC_COMPONENTS})
source_group("Renderer" FILES ${SRC_RENDERER})
source_group("Panels" FILES ${SRC_PANELS})
source_group("Trenchbroom" FILES ${SRC_TRENCHBROOM})
source_group("Geometry" FILES ${SRC_GEOMETRY})
source_group("Level" FILES ${SRC_LEVEL})
source_group("Quake 3" FILES ${SRC_Q3})
source_group("Game UI" FILES ${SRC_GameUI})
source_group("imgui" FILES ${SRC_IMGUI})

target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_XYZW_ONLY)

#set(SHADERS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders")
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
    GIT_TAG        v2.38.0
    SOURCE_SUBDIR  sdk/angelscript/projects/cmake
    UPDATE_DISCONNECTED ON # Запрещает CMake лезть в сеть при каждом сохранении CMakeLists.txt
)

# Скачиваем и подключаем подпроект одной командой
FetchContent_MakeAvailable(angelscript)

FetchContent_Declare(
    nanort
    GIT_REPOSITORY https://github.com/lighttransport/nanort.git
    GIT_TAG        cdf474ae9176b87a9334c858bb06c9e028ed0e0d
    SOURCE_SUBDIR  header-only
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(nanort)

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


############################# JOLT ####################################
# 1. ОТКЛЮЧАЕМ GPU-ВЫЧИСЛЕНИЯ JOLT (Именно эти флаги прописаны в Jolt.cmake)
set(JPH_USE_DX12 OFF CACHE BOOL "Disable DX12 Compute Backend" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "Disable Vulkan Compute Backend" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "Disable Metal Compute Backend" FORCE)

# Включаем обычную CPU-реализацию в качестве заглушки
set(JPH_USE_CPU_COMPUTE ON CACHE BOOL "Use CPU Fallback for Compute" FORCE)

# 2. Стандартные настройки утилит и тестов Jolt
set(ENABLE_SAMPLES OFF CACHE BOOL "Disable Jolt Samples" FORCE)
set(ENABLE_VIEWER OFF CACHE BOOL "Disable Jolt Viewer" FORCE)
set(ENABLE_TESTS OFF CACHE BOOL "Disable Jolt Tests" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE INTERNAL "")
set(TARGET_SAMPLES OFF CACHE INTERNAL "")
set(PROFILER_IMPLEMENTATION "None" CACHE INTERNAL "")

# Загружаем Jolt Physics
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics
    GIT_TAG        v5.6.0
    SOURCE_SUBDIR  Build
    UPDATE_DISCONNECTED ON # Не проверяем обновления в сети без явной команды
)

FetchContent_MakeAvailable(JoltPhysics)
############################# JOLT ####################################

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    vendor/glm
    #vendor/ImGuizmo
    ${angelscript_SOURCE_DIR}/sdk/angelscript/include
    ${nanort_SOURCE_DIR}
    ${joltphysics_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE glad glfw ImGuizmo miniaudio angelscript_addons Jolt)



set(RESOURCES_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources")
set(RESOURCES_LINK_NAME "${CMAKE_BINARY_DIR}/bin/resources")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${RESOURCES_SOURCE_DIR}
        ${RESOURCES_LINK_NAME}
)
