//
//  MeshComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#include "MeshComponent.h"

#include "AnimatorComponent.h"
#include "Entity.h"
#include "ModelLoader.h"
#include "ProjectManager.h"

#include <algorithm>
#include <cctype>

bool MeshComponent::loadModel(const std::filesystem::path& filepath)
{
    auto resource = ModelLoader::LoadModel(filepath.string());

    mesh = resource.mesh;
    materials = resource.embeddedMaterials;

    const auto assetRoot = ProjectManager::Get().assetsPath();
    path = std::filesystem::relative(filepath, assetRoot).generic_string();

    if (resource.hasSkeleton())
    {
        auto animator = owner->getComponent<AnimatorComponent>();

        if (animator == nullptr)
        {
            animator = owner->addComponent<AnimatorComponent>();
        }

        animator->setSkeleton(resource.skeleton);
        animator->m_clips = resource.animations;
        animator->m_currentClipIndex = -1;
        animator->path = path;
    }

    return true;
}

void MeshComponent::save(nlohmann::json& j) const
{
    j["source_glb"] = path;
    j["layer"] = layer;
}

void MeshComponent::load(const nlohmann::json& j)
{
    if(j.contains("source_glb"))
    {
        path = j.at("source_glb");
    }
    
    if(j.contains("layer"))
    {
        layer = j.at("layer");
    }
}
