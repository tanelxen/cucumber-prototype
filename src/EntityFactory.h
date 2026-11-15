//
//  EntityFactory.h
//  Cucumber
//
//  Создание сущностей в сцене. Раньше этот код был скопирован пять раз: три раза в
//  UIManager::drawEntityCreationMenu и по разу в CreateMeshEntity / InstantiatePrefabAt панели вьюпорта.
//
//  Фабрика сцену не выделяет: выбор созданной сущности в UI (UIManager::setEntity) остаётся за вызывающим.
//  Все функции возвращают nullptr, если создать сущность не удалось.
//

#pragma once

#include <filesystem>

#include <glm/glm.hpp>

class WorldScene;
class Entity;

namespace EntityFactory
{
    glm::vec3 SnapPivotToGrid(const glm::vec3& position, float step);

    Entity* CreateEmpty(WorldScene& scene, const glm::vec3& position);
    Entity* CreateLight(WorldScene& scene, const glm::vec3& position);
    Entity* CreateWorldStatic(WorldScene& scene, const glm::vec3& position);

    // gridStep <= 0: позицию не привязывать к сетке
    Entity* CreateFromMesh(WorldScene& scene, const std::filesystem::path& assetPath,
                           const glm::vec3& position, float gridStep);
    Entity* CreateFromPrefab(WorldScene& scene, const std::filesystem::path& assetPath,
                             const glm::vec3& position, float gridStep);
}
