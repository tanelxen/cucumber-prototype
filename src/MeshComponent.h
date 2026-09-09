//
//  MeshComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "Component.h"
#include "Mesh.h"

struct MeshComponent : public Component
{
    std::string path = "<no mesh>";
    
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    
    const std::string getTypeName() const override { return "MeshComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json{ {"source_glb", path} };
    }
    
    void load(const nlohmann::json& j) override
    {
        path = j.at("source_glb");
    }
};
