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

#include <algorithm>
#include <cctype>

bool MeshComponent::IsModelFile(const std::filesystem::path& filepath)
{
    if (!std::filesystem::is_regular_file(filepath))
    {
        return false;
    }

    std::string extension = filepath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".glb" || extension == ".gltf";
}

bool MeshComponent::loadModel(const std::filesystem::path& filepath)
{
    if (!IsModelFile(filepath))
    {
        return false;
    }

    auto resource = ModelLoader::LoadModel(filepath.string());

    mesh = resource.mesh;
    materials = resource.embeddedMaterials;

    const auto assetRoot = std::filesystem::absolute("assets");
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
