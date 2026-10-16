//
//  BSPMeshComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.10.26.
//

#pragma once

#include "Component.h"
#include "RenderDevice.h"

#include <filesystem>

class Q3BspMesh;
class Q3BspCollision;

struct Q3BspComponent : public Component
{
    std::string path = "<no bsp>";
    
    std::shared_ptr<Q3BspMesh> mesh;
    std::shared_ptr<Q3BspCollision> collision;
    uint32_t layer = Layers::Static;
    
    const std::string getTypeName() const override { return "BSPMeshComponent"; }
    
    bool loadQ3BSP(const std::filesystem::path& filepath);
    
    void save(nlohmann::json& j) const override
    {
        j["source_bsp"] = path;
    }
    
    void load(const nlohmann::json& j) override
    {
        if(j.contains("source_bsp"))
        {
            path = j.at("source_bsp");
        }
    }
};
