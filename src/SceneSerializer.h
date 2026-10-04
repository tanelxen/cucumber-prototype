//
//  SceneSerializer.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 05.10.26.
//

#pragma once

#include <string>
#include <vector>
#include "../vendor/json_fwd.hpp"
#include "UID.h"

class WorldScene;
class Entity;

class SceneSerializer
{
public:
    // Главные внешние методы для работы со сценой
    static bool SaveScene(const WorldScene* scene, const std::string& filename);
    static bool LoadScene(WorldScene* scene, const std::string& filename);
    
    static bool CreatePrefabFromEntity(WorldScene* scene, Entity* rootEntity, const std::string& savePath);
    
    // Главный метод для инстанцирования префаба на сцену
    static Entity* InstantiatePrefab(WorldScene* scene, const std::string& filename,
                                     Entity* customParent = nullptr, UID forcedRootUid = UID::Invalid());
    
private:
    static void DeserializeEntity(const nlohmann::json& entityJson, Entity* entity);
};

