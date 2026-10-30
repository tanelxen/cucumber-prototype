//
//  MeshComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "Component.h"
#include "RenderDevice.h"
#include "Mesh.h"

#include <filesystem>

struct MeshComponent : public Component
{
    std::string path = "<no mesh>";
    
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    
    uint32_t layer = RenderLayers::Default;
    
    const std::string getTypeName() const override { return "MeshComponent"; }
    
    bool loadModel(const std::filesystem::path& filepath);

    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
};
