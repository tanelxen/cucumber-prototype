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

struct MeshComponent : public Component
{
    std::string path = "<no mesh>";
    
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    
    uint32_t layer = Layers::Default;
    
    const std::string getTypeName() const override { return "MeshComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j["source_glb"] = path;
        j["layer"] = layer;
    }
    
    void load(const nlohmann::json& j) override
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
};
