//
//  MapComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#pragma once

#include "Component.h"
#include "PhysicsCommon.h"
#include "trenchbroom/LightmapController.h"

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
    bool rebuildLightmapUVs();
    void reload();
    
    bool bakeLighting();                            // запускает фоновый бейк и сразу возвращается
    void cancelBake()          { lightmap.cancel(); }
    bool isBaking() const      { return lightmap.isBaking(); }
    float bakeProgress() const { return lightmap.progress(); }
    bool isLightmapOutdated();
    
    std::string path = "<no map>";
    float lightmapTexelsPerUnit = 1.0f / 8.0f;
    
    std::string bakeStatus;
    LightmapController lightmap{bakeStatus};
    
    std::shared_ptr<MapMesh> mesh;
    
private:
    JPH::BodyID m_bodyID;
    PhysicsUserData m_userData;
    
    void destroyPhysicsBody();
    
private:
    std::vector<LightmapBakeLight> collectBakeLights();
    std::filesystem::path lightmapCachePath() const;
};
