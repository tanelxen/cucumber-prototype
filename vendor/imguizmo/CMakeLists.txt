cmake_minimum_required(VERSION 3.28)
project(ImGuizmo)

# Создаем статическую библиотеку ImGuizmo
add_library(ImGuizmo STATIC 
    ImGuizmo.cpp
    ImGuizmo.h
)

# Настраиваем пути к заголовочным файлам
target_include_directories(ImGuizmo 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE 
        # Поднимаемся на уровень выше, чтобы найти хедеры imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui
)

# Линкуем ImGuizmo с таргетом imgui
target_link_libraries(ImGuizmo PUBLIC imgui)
