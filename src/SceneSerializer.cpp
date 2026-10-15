//
//  SceneSerializer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 05.10.26.
//

#include "SceneSerializer.h"

#include "SceneManager.h"
#include "World.h"

#include "Entity.h"
#include "ExposedProperty.h"
#include "TransformComponent.h"
#include "CollisionComponent.h"
#include "MovementComponent.h"
#include "AnimatorComponent.h"
#include "ScriptComponent.h"
#include "CameraComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

#include "ModelLoader.h"
#include "ProjectManager.h"

//#include <nlohmann/json.hpp>
#include "../vendor/json_fwd.hpp"
#include <fstream>
#include <iostream>

bool SceneSerializer::LoadFile(WorldScene* scene, const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] Не удалось открыть файл: " << filename << std::endl;
        return false;
    }
    
    nlohmann::json rootJson;
    try {
        file >> rootJson;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[SceneSerializer] Ошибка парсинга JSON: " << e.what() << std::endl;
        return false;
    }
    
    if (!rootJson.contains("type") || !rootJson["type"].is_string()) {
        std::cerr << "Неизвестный формат: отсутствует параметр 'type'" << std::endl;
        return false;
    }
    
    scene->clear();
    scene->m_filePath = filename;
    
    std::string type = rootJson["type"];
    
    if (type == "scene")
    {
        return LoadSceneFromMemory(scene, rootJson);
    }
    
    if (type == "prefab")
    {
        scene->m_type = SceneType::Prefab;
        file.close(); // Переоткроем файл внутри InstantiatePrefab
        
        Entity* prefabRoot = InstantiatePrefab(
            scene, ProjectManager::Get().resolveProjectPath(filename).string());
        
        if (prefabRoot)
        {
            // Переводим в режим редактирования исходника префаба: снимаем маркеры копии
            std::function<void(Entity*)> clearPrefabFlags = [&](Entity* ent) {
                ent->prefabAssetPath = "";
                ent->isPrefabRoot = false;
                for (auto* child : ent->getChildren()) { clearPrefabFlags(child); }
            };
            
            clearPrefabFlags(prefabRoot);
            scene->awake();
            return true;
        }
        
        return false;
    }

    return true;
}

Entity* SceneSerializer::InstantiatePrefab(WorldScene* scene, const std::string& filename, Entity* customParent, UID forcedRootUid)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "[Engine] Не удалось открыть файл префаба: " << filename << std::endl;
        return nullptr;
    }
    
    nlohmann::json prefabJson;
    try
    {
        file >> prefabJson;
    }
    catch (const nlohmann::json::parse_error& e)
    {
        std::cerr << "[Engine] Ошибка парсинга JSON префаба: " << e.what() << std::endl;
        return nullptr;
    }
    
    // Поддерживаем как плоский массив, так и объект с полем "entities"
    nlohmann::json entitiesArray = prefabJson.is_array() ? prefabJson : prefabJson["entities"];
    if (entitiesArray.empty()) return nullptr;
    
    Entity* rootEntityPtr = nullptr;
    std::vector<Entity*> instantiatedEntities;
    
    // ЛИНЕЙНЫЙ ЦИКЛ ЗАГРУЗКИ
    for (const auto& entityJson : entitiesArray)
    {
        std::string name = entityJson.value("name", "Entity");
        auto entity = std::make_unique<Entity>(name);
        entity->scene = scene;
        
        entity->prefabAssetPath = filename;
        
        // Загружаем компоненты.
        if (entityJson.contains("components"))
        {
            DeserializeEntity(entityJson, entity.get());
        }
        
        Entity* rawEntityPtr = entity.get();
        instantiatedEntities.push_back(rawEntityPtr);
        scene->m_entities.push_back(std::move(entity)); // Передаем владение сцене
        
        // Восстанавливаем родительские связи на лету
        std::string parentPath = entityJson.value("parent", "");
        if (parentPath.empty())
        {
            // Это корень префаба (он идет первым в файле)
            rootEntityPtr = rawEntityPtr;
            rootEntityPtr->isPrefabRoot = true;
            rootEntityPtr->uid = forcedRootUid.isValid() ? forcedRootUid : UID();
        }
        else
        {
            // Поскольку родитель гарантированно был создан на предыдущих итерациях цикла,
            // мы можем безопасно найти его по относительному пути от корня префаба!
            Entity* parent = rootEntityPtr->findEntityByPath(parentPath);
            if (parent) {
                rawEntityPtr->setParent(parent, false);
            }
        }
    }
    
    // Если передан внешний родитель сцены
    if (customParent && rootEntityPtr) {
        rootEntityPtr->setParent(customParent, false);
    }
    
    // Awake чтобы применить значения из префаба в скриптах
    for (Entity* entity : instantiatedEntities) {
        for (const auto& component : entity->getComponents()) {
            component->awake();
        }
    }
    
    // Заполнение кэша свойств
    if (rootEntityPtr && prefabJson.contains("exposed_properties"))
    {
        for (const auto& propJson : prefabJson["exposed_properties"])
        {
            ExposedProperty meta;
            meta.load(propJson);
            
            // Разрешаем относительный путь в живой указатель ОДИН раз
            Entity* targetEntity = rootEntityPtr->findEntityByPath(meta.targetNodePath);
            if (targetEntity)
            {
                Component* comp = targetEntity->getComponentByName(meta.componentType);
                if (comp)
                {
                    CachedExposedProperty cachedProp;
                    cachedProp.displayName = meta.displayName;
                    cachedProp.propertyName = meta.propertyName;
                    cachedProp.targetComponent = comp; // Прямая ссылка!
                    
                    rootEntityPtr->m_exposedInterfaceCache.push_back(cachedProp);
                }
            }
        }
    }
    
    return rootEntityPtr;
}

bool SceneSerializer::CreatePrefabFromEntity(WorldScene* scene, Entity* rootEntity, const std::string& savePath)
{
    if (!rootEntity) return false;
    
    nlohmann::json prefabJson = nlohmann::json::object();
    prefabJson["type"] = "prefab";
    
    // 1. Создаем дефолтную (базовую) таблицу экспорта.
    prefabJson["exposed_properties"] = {
//        {
//            {"display_name", "Position"},
//            {"target_path", "."},
//            {"component_type", "TransformComponent"},
//            {"property_name", "position"}
//        },
//        {
//            {"display_name", "Rotation"},
//            {"target_path", "."},
//            {"component_type", "TransformComponent"},
//            {"property_name", "rotation"}
//        }
    };
    
    // 2. Собираем плоский список сущностей иерархии
    auto entitiesArray = nlohmann::json::array();
    
    std::function<void(const Entity*)> serializeFlat = [&](const Entity* ent) {
        nlohmann::json entJson;
        entJson["name"] = ent->getName();
        
        if (ent == rootEntity) {
            entJson["parent"] = ""; // Корень префаба внутри файла не имеет родителя
        } else {
            // Вычисляем путь к родителю относительно корня создаваемого префаба
            entJson["parent"] = ent->getParent()->getPathRelativeTo(rootEntity);
        }
        
        // Записываем ВСЕ компоненты сущности полностью (это эталон класса)
        auto componentsArray = nlohmann::json::array();
        
        if (ent != rootEntity)
        {
            auto comp = ent->getTransformPtr();
            
            nlohmann::json compNode;
            compNode["type"] = comp->getTypeName();
            
            nlohmann::json compData;
            comp->save(compData);
            compNode["data"] = compData;
            
            componentsArray.push_back(compNode);
        }
        
        for (auto& comp : ent->getComponents())
        {
            nlohmann::json compNode;
            compNode["type"] = comp->getTypeName();
            nlohmann::json compData = nlohmann::json::object();
            comp->save(compData); // Пишет полные данные
            compNode["data"] = compData;
            componentsArray.push_back(compNode);
        }
        entJson["components"] = componentsArray;
        
        entitiesArray.push_back(entJson);
        
        // Рекурсивно спускаемся к детям, сохраняя правильный порядок "родитель раньше ребенка"
        for (auto* child : ent->getChildren()) {
            serializeFlat(child);
        }
    };
    
    serializeFlat(rootEntity);
    prefabJson["entities"] = entitiesArray;
    
    // 3. Записываем файл на диск
    std::ofstream file(savePath);
    if (!file.is_open()) {
        std::cerr << "[Engine] Не удалось сохранить файл префаба: " << savePath << std::endl;
        return false;
    }
    file << prefabJson.dump(4);
    file.close();
    
    // 4. ПРЕВРАЩАЕМ ЖИВОЙ ОБЪЕКТ НА СЦЕНЕ В ИНСТАНС ПРЕФАБА
    // Чтобы сцена сразу поняла, что этот объект теперь связан с файлом!
    std::function<void(Entity*)> markAsPrefab = [&](Entity* ent) {
        ent->prefabAssetPath = savePath;
        if (ent == rootEntity) {
            ent->isPrefabRoot = true;
            ent->originalPrefabUid = ent->uid; // В рантайме редактора приравниваем исходный ID
        } else {
            ent->isPrefabRoot = false;
        }
        for (auto* child : ent->getChildren()) { markAsPrefab(child); }
    };
    
    markAsPrefab(rootEntity);
    
    // 5. Инициализируем созданный кэш интерфейса для этой живой сущности
    // (Повторяем логику инициализации кэша, так как объект теперь префаб)
    for (const auto& propJson : prefabJson["exposed_properties"])
    {
        CachedExposedProperty cachedProp;
        cachedProp.displayName = propJson["display_name"];
        cachedProp.propertyName = propJson["property_name"];
        // Корень на сцене ("."), компонент TransformComponent всегда на месте
        cachedProp.targetComponent = &rootEntity->getTransform();
        
        rootEntity->m_exposedInterfaceCache.push_back(cachedProp);
    }
    
    std::cout << "[SceneSerializer] Префаб успешно создан и привязан к сцене: " << savePath << std::endl;
    return true;
}

bool SceneSerializer::SaveScene(const WorldScene* scene, const std::string& filename)
{
    nlohmann::json sceneJson;
    if (!scene || !SaveSceneToMemory(scene, sceneJson)) {
        std::cerr << "[Engine] Не удалось подготовить данные сцены для сохранения." << std::endl;
        return false;
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Engine] Не удалось открыть файл сцены для записи: " << filename << std::endl;
        return false;
    }

    file << sceneJson.dump(4);
    if (!file) {
        std::cerr << "[Engine] Не удалось записать файл сцены: " << filename << std::endl;
        return false;
    }

    return true;
}

void SceneSerializer::DeserializeEntity(const nlohmann::json& entityJson, Entity* entity)
{
    auto componentsArray = entityJson.at("components");
    
    for (const auto& compNode : componentsArray)
    {
        std::string compName = compNode.at("type").get<std::string>();
        const nlohmann::json& compData = compNode.at("data");
        
        Component* newComp = nullptr;
        
        // Фабрика компонентов по их текстовому имени
        if (compName == "TransformComponent")
        {
            newComp = &entity->getTransform();
        }
        else if (compName == "MeshComponent")
        {
            newComp = entity->addComponent<MeshComponent>();
            
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel((ProjectManager::Get().assetsPath() / path).string());
            
            auto meshComp = static_cast<MeshComponent*>(newComp);
            meshComp->mesh = model.mesh;
            meshComp->materials = model.embeddedMaterials;
        }
        else if (compName == "AnimatorComponent")
        {
            newComp = entity->addComponent<AnimatorComponent>();
            
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel((ProjectManager::Get().assetsPath() / path).string());
            
            auto animComp = static_cast<AnimatorComponent*>(newComp);
            animComp->setSkeleton(model.skeleton);
            animComp->m_clips = model.animations;
            animComp->path = path; // HACK
        }
        else if (compName == "AudioComponent")
        {
            newComp = entity->addComponent<AudioComponent>();
            
            std::string path = compData.at("source");
            
            auto audioComp = static_cast<AudioComponent*>(newComp);
            audioComp->init((ProjectManager::Get().assetsPath() / path).string());
        }
        else if (compName == "ScriptComponent")
        {
            newComp = entity->addComponent<ScriptComponent>();
        }
        else if (compName == "CameraComponent")
        {
            newComp = entity->addComponent<CameraComponent>();
        }
        else if (compName == "MovementComponent")
        {
            newComp = entity->addComponent<MovementComponent>();
        }
        else if (compName == "CollisionComponent")
        {
            newComp = entity->addComponent<CollisionComponent>();
        }
        
        // Загружаем уникальное состояние
        if (newComp)
        {
            newComp->load(compData);
        }
    }
}

bool SceneSerializer::SaveSceneToMemory(const WorldScene* scene, nlohmann::json& sceneJson)
{
    auto entitiesArray = nlohmann::json::array();
    
    for (const auto& entity : scene->m_entities)
    {
        // Дочерние объекты префабов пропускаем, они запишутся внутри своего корня в блок overrides
        if (entity->isPrefabChild()) continue;
        
        nlohmann::json entityJson;
        entityJson["uid"] = entity->uid;
        entityJson["name"] = entity->getName();
        
        if (entity->getParent()) {
            entityJson["parent"] = entity->getParent()->uid;
        }
        
        if (entity->isPrefabRoot)
        {
            // СЛУЧАЙ 1: Это экземпляр префаба
            entityJson["prefab_path"] =
                ProjectManager::Get().makeProjectRelative(entity->prefabAssetPath);
            
            // ПРИНУДИТЕЛЬНО сохраняем трансформацию именно этого экземпляра на сцене
            auto transformJson = nlohmann::json::object();
            entity->getTransform().save(transformJson);
            entityJson["transform_override"] = transformJson;
            
            nlohmann::json interfaceOverridesJson = nlohmann::json::object();
            
            // Проходим по нашему быстрому кэшу экспортированных свойств
            for (const auto& prop : entity->m_exposedInterfaceCache)
            {
                // Если свойство было изменено на сцене (флаг взведен в ImGui)
                if (entity->isInterfaceOverrideDirty(prop.displayName))
                {
                    if (prop.targetComponent)
                    {
                        // Берем текущее значение и сохраняем его в файл сцены под внешним именем
                        interfaceOverridesJson[prop.displayName] =
                        prop.targetComponent->getExposedValue(prop.propertyName);
                    }
                }
            }
            
            if (!interfaceOverridesJson.empty()) {
                entityJson["interface_overrides"] = interfaceOverridesJson;
            }
        }
        else
        {
            // СЛУЧАЙ 2: Это обычный, уникальный объект сцены
            // Сохраняем все его компоненты полностью
            
            auto componentsArray = nlohmann::json::array();
            
            {
                auto comp = entity->getTransformPtr();
                
                nlohmann::json compNode;
                compNode["type"] = comp->getTypeName();
                
                nlohmann::json compData;
                comp->save(compData);
                compNode["data"] = compData;
                
                componentsArray.push_back(compNode);
            }
            
            for (const auto& comp : entity->getComponents())
            {
                nlohmann::json compNode;
                compNode["type"] = comp->getTypeName();
                
                nlohmann::json compData;
                comp->save(compData);
                compNode["data"] = compData;
                
                componentsArray.push_back(compNode);
            }
            
            entityJson["components"] = componentsArray;
        }
        
        entitiesArray.push_back(entityJson);
    }
    
    sceneJson["entities"] = entitiesArray;
    sceneJson["type"] = "scene";
    
    return true;
}

bool SceneSerializer::LoadSceneFromMemory(WorldScene* scene, const nlohmann::json& sceneJson)
{
    scene->clear();
    scene->m_type = SceneType::Level;
    
    std::unordered_map<UID, Entity*> entityMap;
    // Мапа для последующего восстановления связей parent-child обычных сущностей
    std::vector<std::pair<Entity*, UID>> nonPrefabParentsToResolve;
    
    // ==========================================
    // ПРОХОД 1: Создание Entity и их компонентов
    // ==========================================
    for (const auto& entityJson : sceneJson["entities"])
    {
        if (entityJson.contains("prefab_path"))
        {
            std::string prefabPath = ProjectManager::Get().resolveProjectPath(
                entityJson["prefab_path"].get<std::string>()).string();
            UID savedRootUid = entityJson["uid"].get<UID>();
            
            // 1. Просто спавним чистую структуру из префаба.
            Entity* prefabRoot = InstantiatePrefab(scene, prefabPath, nullptr, savedRootUid);
            
            if (!prefabRoot) continue;
            
            // 2. Если спавн прошел успешно и в сцене сохранены уникальные трансформации
            if (prefabRoot && entityJson.contains("transform_override"))
            {
                // Перезаписываем дефолтный трансформ сценовыми данными
                prefabRoot->getTransformPtr()->load(entityJson["transform_override"]);
            }
            
            // Читаем метаданные префаба, чтобы знать его интерфейс (таблицу экспорта)
            nlohmann::json prefabFileJson;
            std::ifstream pFile(prefabPath);
            pFile >> prefabFileJson;
            
            std::vector<ExposedProperty> exposedInterface;
            if (prefabFileJson.contains("exposed_properties"))
            {
                for (const auto& propJson : prefabFileJson["exposed_properties"])
                {
                    ExposedProperty prop;
                    prop.load(propJson);
                    exposedInterface.push_back(prop);
                }
            }
            
            // 3. Накатываем оверрайды интерфейса из файла сцены
            if (entityJson.contains("interface_overrides"))
            {
                const auto& sceneOverrides = entityJson["interface_overrides"];
                
                for (const auto& prop : exposedInterface)
                {
                    // Если сцена переопределила это внешнее свойство
                    if (sceneOverrides.contains(prop.displayName))
                    {
                        nlohmann::json newValue = sceneOverrides[prop.displayName];
                        
                        // Находим целевую сущность внутри префаба по её NodePath!
                        Entity* targetEntity = prefabRoot->findEntityByPath(prop.targetNodePath);
                        if (targetEntity)
                        {
                            // Находим нужный компонент на этой сущности
                            Component* comp = targetEntity->getComponentByName(prop.componentType);
                            if (comp)
                            {
                                // Записываем значение напрямую во внутренность префаба!
                                comp->setExposedValue(prop.propertyName, newValue);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            std::string name = entityJson.value("name", "Unnamed_Entity");
//            std::cout << "Load " << name << std::endl;
            
            auto entity = std::make_unique<Entity>(name);
            entity->uid = entityJson.at("uid").get<UID>();
            entity->scene = scene;
            
            if (entityJson.contains("parent"))
            {
                UID parentId = entityJson.at("parent").get<UID>();
                
                if (parentId != 0) {
                    nonPrefabParentsToResolve.push_back({entity.get(), parentId});
                }
            }
            
            if (entityJson.contains("components"))
            {
                DeserializeEntity(entityJson, entity.get());
            }
            
            entityMap[entity->uid] = entity.get();
            scene->m_entities.push_back(std::move(entity));
        }
    }
    
    // Восстанавливаем parent-child связи для ОБЫЧНЫХ сущностей сцены
    for (auto& [child, parentUID] : nonPrefabParentsToResolve)
    {
        Entity* parent = scene->findEntityByUID(parentUID);
        if (parent && child) child->setParent(parent, false);
    }
    
    // ВСЕ сущности созданы, ВСЕ parent-child связи восстановлены.
    // Теперь безопасно вызываем awake() для абсолютно всех компонентов на сцене:
    for (auto& entity : scene->m_entities)
    {
        entity->awake();
    }
    
    return true;
}
