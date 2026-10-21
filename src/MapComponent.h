//
//  MapComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#pragma once

#include "Component.h"
#include "PhysicsCommon.h"

#include <Jolt/Physics/Body/BodyID.h>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

class MapMesh;
struct MapAsset;

struct MapComponent : public Component
{
    ~MapComponent();
    
    const std::string getTypeName() const override { return "MapComponent"; }
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    bool loadTrenchBroomMap(const std::filesystem::path& filepath);
    void reload();
    
    std::string path = "<no map>";
    std::shared_ptr<MapMesh> mesh;
    
private:
    JPH::BodyID m_bodyID;
    PhysicsUserData m_userData;
    
    void destroyPhysicsBody();
};
