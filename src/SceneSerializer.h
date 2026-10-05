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
    // Главная точка входа для загрузки любого файла (сцены или префаба)
    // Возвращает true, если файл успешно прочитан и применен к WorldScene
    static bool LoadFile(WorldScene* scene, const std::string& filename);
    
    // Главные внешние методы для работы со сценой
    static bool SaveScene(const WorldScene* scene, const std::string& filename);
//    static bool LoadScene(WorldScene* scene, const std::string& filename);
    
    static bool SaveSceneToMemory(const WorldScene* scene, nlohmann::json& outJson);
    static bool LoadSceneFromMemory(WorldScene* scene, const nlohmann::json& inJson);
    
    static bool CreatePrefabFromEntity(WorldScene* scene, Entity* rootEntity, const std::string& savePath);
    
    // Главный метод для инстанцирования префаба на сцену
    static Entity* InstantiatePrefab(WorldScene* scene, const std::string& filename,
                                     Entity* customParent = nullptr, UID forcedRootUid = UID::Invalid());
    
private:
    static void DeserializeEntity(const nlohmann::json& entityJson, Entity* entity);
    static void HotReloadPrefabInstance(WorldScene* levelScene, Entity* rootEntity);
};

