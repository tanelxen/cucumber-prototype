//
//  EntityFactory.cpp
//  Cucumber
//

#include "EntityFactory.h"

#include "Entity.h"
#include "LightComponent.h"
#include "MapComponent.h"
#include "MeshComponent.h"
#include "ModelLoader.h"
#include "SceneSerializer.h"
#include "World.h"

#include <iostream>
#include <string>

namespace
{
    Entity* Spawn(WorldScene& scene, const std::string& name, const glm::vec3& position)
    {
        auto entity = std::make_unique<Entity>(name);
        entity->scene = &scene;
        entity->getTransform().setPosition(position);

        Entity* created = entity.get();
        scene.m_entities.push_back(std::move(entity));
        return created;
    }

    std::string DefaultName(const WorldScene& scene)
    {
        return "Entity_" + std::to_string(scene.m_entities.size());
    }

    glm::vec3 MaybeSnap(const glm::vec3& position, float gridStep)
    {
        return gridStep > 0.0f ? EntityFactory::SnapPivotToGrid(position, gridStep) : position;
    }
}

namespace EntityFactory
{
    glm::vec3 SnapPivotToGrid(const glm::vec3& position, float step)
    {
        return glm::round(position / step) * step;
    }

    Entity* CreateEmpty(WorldScene& scene, const glm::vec3& position)
    {
        return Spawn(scene, DefaultName(scene), position);
    }

    Entity* CreateLight(WorldScene& scene, const glm::vec3& position)
    {
        Entity* entity = Spawn(scene, DefaultName(scene), position);
        entity->addComponent<LightComponent>();
        return entity;
    }

    Entity* CreateWorldStatic(WorldScene& scene, const glm::vec3& position)
    {
        Entity* entity = Spawn(scene, "Worldspawn", position);
        entity->addComponent<MapComponent>();
        return entity;
    }

    Entity* CreateFromMesh(WorldScene& scene, const std::filesystem::path& assetPath,
                           const glm::vec3& position, float gridStep)
    {
        // Предварительная проверка, что ресурс вообще грузится (как было в панели вьюпорта)
        const ModelResource resource = ModelLoader::LoadModel(assetPath.string());
        if (!resource.mesh)
        {
            std::cerr << "[EntityFactory] Failed to load mesh asset: " << assetPath << '\n';
            return nullptr;
        }

        Entity* entity = Spawn(scene, "Mesh_" + assetPath.stem().string(), position);
        auto* mesh = entity->addComponent<MeshComponent>();
        if (!mesh->loadModel(assetPath))
        {
            std::cerr << "[EntityFactory] Failed to initialize mesh asset: " << assetPath << '\n';
            entity->destroy();
            return nullptr;
        }

        entity->getTransform().setPosition(MaybeSnap(position, gridStep));
        return entity;
    }

    Entity* CreateFromPrefab(WorldScene& scene, const std::filesystem::path& assetPath,
                             const glm::vec3& position, float gridStep)
    {
        Entity* root = SceneSerializer::InstantiatePrefab(&scene, assetPath.string(), nullptr, UID::Invalid(), false);
        if (!root)
        {
            std::cerr << "[EntityFactory] Failed to instantiate prefab: " << assetPath << '\n';
            return nullptr;
        }

        root->getTransform().setPosition(MaybeSnap(position, gridStep));
        SceneSerializer::InitializePrefabComponents(root);
        return root;
    }
}
