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

set(SRC_CORE
    src/core/main.cpp
    src/core/Application.cpp
    src/core/Application.h
    src/core/ProjectManager.cpp
    src/core/ProjectManager.h
    src/core/Input.cpp
    src/core/Input.h
    src/core/UID.cpp
    src/core/UID.h
    src/core/SpatialGrid.h
    src/core/Bounds.h
)

set(SRC_ASSETS
     src/assets/ModelLoader.cpp
     src/assets/ModelLoader.h
     src/assets/Skeleton.h
     src/assets/Animation.h
     src/assets/TextureManager.cpp
     src/assets/TextureManager.h
     src/assets/MapAsset.cpp
     src/assets/MapAsset.h
     src/assets/MapImporter.cpp
     src/assets/MapImporter.h
     vendor/tinyfiledialogs.c
)

set(SRC_SCENE
    src/scene/World.cpp
    src/scene/World.h
    src/scene/Entity.cpp
    src/scene/Entity.h
    src/scene/ExposedProperty.h
    src/scene/SceneSerializer.cpp
    src/scene/SceneSerializer.h
    src/scene/SceneManager.cpp
    src/scene/SceneManager.h
    src/scene/CameraManager.cpp
    src/scene/CameraManager.h
    src/scene/SceneExtractor.cpp
    src/scene/SceneExtractor.h
)

set(SRC_COMPONENTS
    src/components/Component.h
    src/components/AnimatorComponent.cpp
    src/components/AnimatorComponent.h
    src/components/AudioComponent.cpp
    src/components/AudioComponent.h
    src/components/MeshComponent.cpp
    src/components/MeshComponent.h
    src/components/TransformComponent.cpp
    src/components/TransformComponent.h
    src/components/ScriptComponent.cpp
    src/components/ScriptComponent.h
    src/components/CameraComponent.cpp
    src/components/CameraComponent.h
    src/components/MovementComponent.cpp
    src/components/MovementComponent.h
    src/components/CollisionComponent.cpp
    src/components/CollisionComponent.h
    src/components/MapComponent.cpp
    src/components/MapComponent.h
    src/components/RigidbodyComponent.cpp
    src/components/RigidbodyComponent.h
    src/components/LightComponent.cpp
    src/components/LightComponent.h
)

set(SRC_RENDERER
    src/renderer/RenderDevice.cpp
    src/renderer/RenderDevice.h
    src/renderer/VertexArray.cpp
    src/renderer/VertexArray.h
    src/renderer/GpuBuffer.cpp
    src/renderer/GpuBuffer.h
    src/renderer/VertexBuffer.cpp
    src/renderer/VertexBuffer.h
    src/renderer/VertexLayout.cpp
    src/renderer/VertexLayout.h
    src/renderer/IndexBuffer.cpp
    src/renderer/IndexBuffer.h
    src/renderer/RenderTarget.cpp
    src/renderer/RenderTarget.h
    src/renderer/Texture.cpp
    src/renderer/Texture.h
    src/renderer/Mesh.cpp
    src/renderer/Mesh.h
    src/renderer/Shader.cpp
    src/renderer/Shader.h
    src/renderer/ShaderVariant.cpp
    src/renderer/ShaderVariant.h
    src/renderer/GizmoRender.cpp
    src/renderer/GizmoRender.h
    src/renderer/SpriteRenderer.cpp
    src/renderer/SpriteRenderer.h
    src/renderer/ParticleSystem.cpp
    src/renderer/ParticleSystem.h
    src/renderer/DecalManager.cpp
    src/renderer/DecalManager.h
        
    src/renderer/Renderer.cpp
    src/renderer/Renderer.h
    src/renderer/IViewProvider.h
    src/renderer/GameViewProducer.cpp
    src/renderer/GameViewProducer.h
    src/renderer/ViewDesc.h
    src/renderer/RenderScene.cpp
    src/renderer/RenderScene.h
    src/renderer/ViewBuilder.cpp
    src/renderer/ViewBuilder.h
    src/renderer/ShadowSystem.cpp
    src/renderer/ShadowSystem.h
    src/renderer/ShadowTypes.h
        
    src/renderer/OverlayRender.cpp
    src/renderer/OverlayRender.h
)

set(SRC_GameUI
    src/ui/GameUI.cpp
    src/ui/GameUI.h
    src/ui/GameUIScreen.cpp
    src/ui/GameUIScreen.h
    src/ui/GameUI_AS.cpp
    src/ui/GameUI_AS.h
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
    src/level/BlockTool.cpp
    src/level/BlockTool.h
    src/level/VertexTool.cpp
    src/level/VertexTool.h
    src/level/EditMath.cpp
    src/level/EditMath.h
    
    src/level/MapMesh.cpp
    src/level/MapMesh.h

    src/level/LevelCooker.cpp
    src/level/LevelCooker.h
    
    src/level/CookCommon.h
    src/level/EditGeometry.cpp
    src/level/EditGeometry.h
    src/level/LevelEditMesh.cpp
    src/level/LevelEditMesh.h
    
    src/level/GeometryRevision.h
    src/level/ISurfaceGeometry.h
    src/level/MaterialTable.h
    
    src/level/LevelDocument.cpp
    src/level/LevelDocument.h
    src/level/LevelDocumentIO.cpp
    src/level/LevelDocumentIO.h
    src/level/LevelEditOverlay.cpp
    src/level/LevelEditOverlay.h
    src/level/LevelEditSession.cpp
    src/level/LevelEditSession.h
    src/level/LevelHistory.cpp
    src/level/LevelHistory.h
    src/level/LevelPicking.cpp
    src/level/LevelPicking.h

    src/level/LevelTool.cpp
    src/level/LevelTool.h
    src/level/SelectTool.cpp
    src/level/SelectTool.h
    src/level/SolidBuilder.cpp
    src/level/SolidBuilder.h
    src/level/ViewportPointer.h
    
    src/level/FaceTexturing.cpp
    src/level/FaceTexturing.h
    
    src/level/LevelClipboard.cpp
    src/level/LevelClipboard.h
    
    src/level/LevelWireframe.cpp
    src/level/LevelWireframe.h
)

set(SRC_LIGHTMAP
    src/lightmap/BakeLights.cpp
    src/lightmap/BakeLights.h
    src/lightmap/LightmapBaker.cpp
    src/lightmap/LightmapBaker.h
    src/lightmap/LightmapBakeService.cpp
    src/lightmap/LightmapBakeService.h
    src/lightmap/LightmapCache.cpp
    src/lightmap/LightmapCache.h
    src/lightmap/LightmapController.cpp
    src/lightmap/LightmapController.h
    src/lightmap/Parallel.h
    src/lightmap/Half.h
)

set(SRC_SCRIPTING
    src/scripting/ScriptEngine.cpp
    src/scripting/ScriptEngine.h
    src/scripting/AngelScriptBackend.cpp
    src/scripting/AngelScriptRuntime.cpp
    src/scripting/AngelScriptRuntime.h
    src/scripting/ScriptRuntime.h
)

set(SRC_PHYSICS
    src/physics/MapPhysicsBody.cpp
    src/physics/MapPhysicsBody.h
    src/physics/PhysicsCommon.h
    src/physics/PhysicsContactListener.cpp
    src/physics/PhysicsContactListener.h
    src/physics/PhysicsSystem.cpp
    src/physics/PhysicsSystem.h
)

set(SRC_AUDIO
    src/audio/AudioSystem.h
)

set(SRC_EDITOR
    src/editor/TextureLibrary.cpp
    src/editor/TextureLibrary.h
    
    src/editor/UIManager.cpp
    src/editor/UIManager.h

    src/editor/ViewportPanel.cpp
    src/editor/ViewportPanel.h
    src/editor/InspectorPanel.cpp
    src/editor/InspectorPanel.h
    src/editor/ComponentInspector.cpp
    src/editor/ComponentInspector.h
    src/editor/AssetsPanel.cpp
    src/editor/AssetsPanel.h
    src/editor/HierarchyPanel.cpp
    src/editor/HierarchyPanel.h
    
    src/editor/EditorCamera.cpp
    src/editor/EditorCamera.h
    src/editor/EditorViewport.cpp
    src/editor/EditorViewport.h
    
    src/editor/TextureBrowserPanel.cpp
    src/editor/TextureBrowserPanel.h
    src/editor/TexturingPanel.cpp
    src/editor/TexturingPanel.h
    
    src/editor/EntityFactory.cpp
    src/editor/EntityFactory.h
    src/editor/ViewportPicking.cpp
    src/editor/ViewportPicking.h
    
    src/editor/EditorWidgets.cpp
    src/editor/EditorWidgets.h
    src/editor/LevelEditInput.cpp
    src/editor/LevelEditInput.h
    src/editor/ViewportAssetDrop.cpp
    src/editor/ViewportAssetDrop.h
    src/editor/ViewportGizmo.cpp
    src/editor/ViewportGizmo.h
    src/editor/ViewportImageRect.h
    src/editor/ViewportMouse.cpp
    src/editor/ViewportMouse.h
    src/editor/ViewportToolbar.cpp
    src/editor/ViewportToolbar.h
    
    src/editor/LevelEditState.cpp
    src/editor/LevelEditState.h
    src/editor/LevelFiles.cpp
    src/editor/LevelFiles.h

    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable( ${PROJECT_NAME}
    ${SRC_CORE}
    ${SRC_ASSETS}
    ${SRC_SCENE}
    ${SRC_COMPONENTS}
    ${SRC_RENDERER}
    ${SRC_GameUI}
    ${SRC_GEOMETRY}
    ${SRC_LEVEL}
    ${SRC_LIGHTMAP}
    ${SRC_SCRIPTING}
    ${SRC_PHYSICS}
    ${SRC_AUDIO}
    ${SRC_EDITOR}
)

source_group("Core" FILES ${SRC_CORE})
source_group("Assets" FILES ${SRC_ASSETS})
source_group("Scene" FILES ${SRC_SCENE})
source_group("Components" FILES ${SRC_COMPONENTS})
source_group("Renderer" FILES ${SRC_RENDERER})
source_group("Geometry" FILES ${SRC_GEOMETRY})
source_group("Level" FILES ${SRC_LEVEL})
source_group("Lightmap" FILES ${SRC_LIGHTMAP})
source_group("Scripting" FILES ${SRC_SCRIPTING})
source_group("Physics" FILES ${SRC_PHYSICS})
source_group("Audio" FILES ${SRC_AUDIO})
source_group("Game UI" FILES ${SRC_GameUI})
source_group("Editor" FILES ${SRC_EDITOR})

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
