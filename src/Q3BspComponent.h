//
//  BSPMeshComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.10.26.
//

#pragma once

#include "Component.h"
#include "PhysicsCommon.h"

#include <Jolt/Physics/Body/BodyID.h>

#include <filesystem>

class Q3BspMesh;
class Q3BspCollision;

struct Q3BspComponent : public Component
{
    ~Q3BspComponent();
    
    const std::string getTypeName() const override { return "BSPMeshComponent"; }
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    bool loadQ3BSP(const std::filesystem::path& filepath);
    
    std::string path = "<no bsp>";
    std::shared_ptr<Q3BspMesh> mesh;
    
private:
    JPH::BodyID m_bodyID;
    PhysicsUserData m_userData;
    
    void destroyPhysicsBody();
};
