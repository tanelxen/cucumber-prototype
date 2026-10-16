//
//  ModelLoader.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.07.26.
//

#pragma once

#include <string>
#include <vector>
#include <memory>

class Mesh;
class Material;
class Skeleton;
class AnimationClip;

struct ModelResource
{
    std::string filepath;
    std::shared_ptr<Mesh> mesh = nullptr;
    std::shared_ptr<Skeleton> skeleton = nullptr;
    std::vector<std::shared_ptr<AnimationClip>> animations;
    std::vector<std::shared_ptr<Material>> embeddedMaterials;
    
    bool hasSkeleton() const { return skeleton != nullptr; }
    bool hasAnimations() const { return !animations.empty(); }
};

namespace ModelLoader {
    ModelResource LoadModel(const std::string& filepath);
};
