//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

#include "Application.h"
#include "UIManager.h"
#include "InspectorPanel.h"

#include "Camera.h"

#include "Grid.h"
#include "ModelLoader.h"

#include "Entity.h"
#include "TransformComponent.h"
#include "AnimatorComponent.h"
#include "ScriptComponent.h"
#include "CameraComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

#include "Viewport.h"

#include "RenderDevice.h"
#include "AudioSystem.h"
#include "ScriptEngine.h"

#include "Sky.h"

#include "../vendor/json.hpp"

#include <iostream>
#include <fstream>

static Grid g_grid;
static Sky g_sky;

WorldScene::WorldScene()
{
    g_grid.init();
    g_sky.init();
    AudioSystem::Get().Initialize();
    
    if (ScriptEngine::get().init())
    {
        ScriptEngine::get().CompileAllScripts("assets/scripts");
    }
    else
    {
        std::cerr << "Failed to init AngelScript!" << std::endl;
    }
    
    
    g_currentScene = this;
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    processPendingStarts();
    
    for (auto* component : m_globalUpdatables)
    {
        if (component->owner->m_isActive && component->enabled) {
            component->update(dt);
        }
    }
    
    // 2. СНАЧАЛА зачищаем указатели «смертников» из систем обновлений Сцены
    cleanupDeadComponents();
    
    // 3. И ТОЛЬКО ПОТОМ физически удаляем сущности из памяти а с ними и сами компоненты
    cleanupDeadEntities();
}

void WorldScene::draw()
{
    RenderDevice::startFrame();
    
    if (m_drawGrid)
    {
        g_grid.draw();
    }
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
//        if (entity->isPendingKill()) continue;
        
        auto transformComp = entity->getComponent<TransformComponent>();
        auto meshComp = entity->getComponent<MeshComponent>();
        
        auto model = transformComp ? transformComp->toMat4() : glm::mat4(1.0);
        
        if (meshComp && meshComp->mesh)
        {
            auto animator = entity->getComponent<AnimatorComponent>();
            
            if (!animator || animator->m_clips.empty())
            {
                meshComp->mesh->draw(model);
            }
            else
            {
                meshComp->mesh->draw(model, animator->getFinalBoneMatrices());
            }
        }
    }
    
    // FIXME: установка вьюпорта быть в RenderDevice
    m_pAttachedViewport->bind();
    
    if (m_drawSkybox)
    {
        auto camera = m_pAttachedViewport->getCamera();
        g_sky.draw(camera->view, camera->projection);
    }
    
    RenderDevice::commit(m_pAttachedViewport->getCamera());
    
    m_pAttachedViewport->unbind();
    
    RenderDevice::endFrame();
}

void WorldScene::selectEntityAtIndex(int index)
{
    if (index < m_entities.size())
    {
        Entity* rawPtr = m_entities[index].get();
        Application::currentInstance->m_pUIManager->setEntity(rawPtr);
    }
}

using json = nlohmann::json;

void to_json(json& entityJson, const Entity& entity)
{
    entityJson["name"] = entity.m_name;
    entityJson["active"] = entity.m_isActive;
    
    json componentsJson = json::object();
    
    for (const auto& comp : entity.getComponents())
    {
        json compData;
        comp->save(compData);
        
        // Ключом будет имя компонента, например "TransformComponent"
        componentsJson[comp->getTypeName()] = compData;
    }
    
    entityJson["components"] = componentsJson;
}

static constexpr uint8_t uuid_characters[] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    '0', '1', '2', '3', '4', '5', '6', '7', '8'
};

template <typename T, size_t SIZE>
constexpr size_t std_size(const T (&)[SIZE]) {
    return SIZE;
}

static constexpr uint32_t base = std_size(uuid_characters); // Строго 34
static constexpr uint32_t max_uuid_number_length = 13;

std::string id_to_text(int64_t p_id)
{
    if (p_id < 0) {
        return "uid://<invalid>";
    }
    
    char32_t tmp[max_uuid_number_length];
    uint32_t tmp_size = 0;
    
    uint64_t remaining_id = static_cast<uint64_t>(p_id);
    
    do {
        uint32_t c = remaining_id % base;
        tmp[tmp_size] = uuid_characters[c];
        remaining_id /= base;
        ++tmp_size;
    } while (remaining_id);
    
    std::string txt;
    // +6 для "uid://" и +1 для '\0' (resize не требует +1 для '\0', но мы заполняем вручную)
    txt.resize(tmp_size + 6);
    
    txt[0] = 'u';
    txt[1] = 'i';
    txt[2] = 'd';
    txt[3] = ':';
    txt[4] = '/';
    txt[5] = '/';
    
    uint32_t size = 6;
    for (uint32_t i = 0; i < tmp_size; ++i) {
        txt[size++] = tmp[tmp_size - i - 1];
    }
    
    return txt;
}

int64_t text_to_id(const std::string &p_text)
{
    // Проверяем префикс и минимальную длину (хотя бы 1 символ после "uid://")
    if (p_text.length() <= 6 || !p_text.starts_with("uid://") || p_text == "uid://<invalid>") {
        return -1; // Возвращаем -1 (или 0) для невалидного ID
    }
    
    uint32_t l = p_text.length();
    uint64_t uid = 0;
    
    for (uint32_t i = 6; i < l; i++) {
        uint32_t c = p_text[i];
        
        // Ищем символ в нашем же массиве алфавита
        const uint8_t* slot = std::find(std::begin(uuid_characters), std::end(uuid_characters), c);
        
        if (slot == std::end(uuid_characters)) {
            return -1; // Найден символ, которого нет в алфавите
        }
        
        uint32_t index = std::distance(std::begin(uuid_characters), slot);
        
        uid *= base;
        uid += index;
    }
    
    // Возвращаем как int64_t, маскируя старший бит для безопасности знака
    return static_cast<int64_t>(uid & 0x7FFFFFFFFFFFFFFF);
}

// Алфавит Base32 (32 символа, исключены опасные 0, 1, 8, 9)
const char BASE32_ALPHABET[] = "ABCDEFGHJKMNPQRSTVWXYZ23456789";

// Кодирование uint64_t в Base32 строку
std::string ToBase32(uint64_t value) {
    if (value == 0) return std::string(1, BASE32_ALPHABET[0]);
    
    std::string result;
    result.reserve(13); // Максимум 13 символов для uint64_t
    
    while (value > 0) {
        result.push_back(BASE32_ALPHABET[value & 0x1F]); // Берем последние 5 бит (32 значения)
        value >>= 5; // Сдвигаем на 5 бит вправо
    }
    
    // Разворачиваем строку, так как мы читали с конца
    std::reverse(result.begin(), result.end());
    return result;
}

// Декодирование обратно в uint64_t
uint64_t FromBase32(const std::string& str) {
    uint64_t result = 0;
    for (char c : str) {
        // Приводим к верхнему регистру (для регистронезависимости)
        char up_c = std::toupper(c);
        
        // Ищем индекс символа в алфавите
        const char* slot = std::find(std::begin(BASE32_ALPHABET), std::end(BASE32_ALPHABET), up_c);
        if (slot == std::end(BASE32_ALPHABET)) continue; // Пропускаем невалидные символы
        
        size_t index = std::distance(std::begin(BASE32_ALPHABET), slot);
        result = (result << 5) | (index & 0x1F);
    }
    return result;
}

void SerializeEntity(json& entityJson, const Entity* entity)
{
    entityJson["name"] = entity->m_name;
    entityJson["uid"] = entity->uid;
    entityJson["parent"] = entity->parent ? entity->parent->uid : UID::Invalid();
    
    json componentsArray = json::array();
    
    for (const auto& comp : entity->getComponents())
    {
        json compNode;
        compNode["type"] = comp->getTypeName();
        
        json compData;
        comp->save(compData);
        compNode["data"] = compData;
        
        componentsArray.push_back(compNode);
    }
    
    entityJson["components"] = componentsArray;
}

void WorldScene::save()
{
    json sceneJson;
    
    sceneJson["name"] = "test";
    sceneJson["entities"] = json::array();
    
    for (const auto& entity : m_entities)
    {
        json entityJson;
        SerializeEntity(entityJson, entity.get());
        sceneJson["entities"].push_back(entityJson);
    }
    
    std::ofstream file("scene.json");
    file << sceneJson.dump(4);
}

static void DeserializeEntity(const json& entityJson, Entity* entity)
{
    auto componentsArray = entityJson.at("components");
    
    for (const auto& compNode : componentsArray)
    {
        std::string compName = compNode.at("type").get<std::string>();
        const json& compData = compNode.at("data");
        
        Component* newComp = nullptr;
        
        // Фабрика компонентов по их текстовому имени
        if (compName == "TransformComponent")
        {
            newComp = entity->addComponent<TransformComponent>();
        }
        else if (compName == "MeshComponent")
        {
            newComp = entity->addComponent<MeshComponent>();
            
            // После создания компонента, не забудьте накатить на него ресурсы!
            // Берём путь к glb, который прочитается внутри Deserialize, и грузим через ResourceManager
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel(path);
            
            auto meshComp = static_cast<MeshComponent*>(newComp);
            meshComp->mesh = model.mesh;
            meshComp->materials = model.embeddedMaterials;
        }
        else if (compName == "AnimatorComponent")
        {
            newComp = entity->addComponent<AnimatorComponent>();
            
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel(path);
            
            auto animComp = static_cast<AnimatorComponent*>(newComp);
            animComp->setSkeleton(model.skeleton);
            animComp->m_clips = model.animations;
        }
        else if (compName == "AudioComponent")
        {
            newComp = entity->addComponent<AudioComponent>();
            
            std::string path = compData.at("source");
            
            auto audioComp = static_cast<AudioComponent*>(newComp);
            audioComp->init(path);
        }
        else if (compName == "ScriptComponent")
        {
            newComp = entity->addComponent<ScriptComponent>();
        }
        
        // Загружаем уникальное состояние (время, позицию, скорость и т.д.)
        if (newComp)
        {
            newComp->load(compData);
        }
    }
}

void WorldScene::load()
{
    std::string filename = "scene.json";
    
    // 1. Открываем и читаем JSON-файл
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл сцены: " << filename << std::endl;
        return;
    }
    
    json sceneJson;
    try {
        file >> sceneJson;
    } catch (const json::parse_error& e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        return;
    }
    
    // Проверяем базовую структуру файла
    if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
        std::cerr << "Неверный формат сцены: отсутствует массив 'entities'" << std::endl;
        return;
    }
    
    std::unordered_map<UID, Entity*> entityMap;
    
    // ==========================================
    // ПРОХОД 1: Создание Entity и их компонентов
    // ==========================================
    for (const auto& entityJson : sceneJson["entities"])
    {
        std::string name = entityJson.value("name", "Unnamed_Entity");
        std::cout << "Load " << name << std::endl;

        auto entity = std::make_unique<Entity>(name);
        entity->uid = entityJson.at("uid").get<UID>();
        
        if (entityJson.contains("components"))
        {
            DeserializeEntity(entityJson, entity.get());
        }
        
        entityMap[entity->uid] = entity.get();
        m_entities.push_back(std::move(entity));
    }
    
    // ВТОРОЙ ПРОХОД: Восстанавливаем связи иерархии Parent-Child по ID
    for (const auto& j : sceneJson["entities"])
    {
        UID entityId = j.at("uid").get<UID>();
        UID parentId = j.at("parent").get<UID>();
        
        if (parentId != 0)
        {
            Entity* child = entityMap[entityId];
            Entity* parent = entityMap[parentId];
            
            if (child && parent) {
                child->setParent(parent); // Связываем их!
            }
        }
    }
}

Entity *WorldScene::findEntityByName(const std::string &name)
{
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
                           [&name](const std::unique_ptr<Entity>& entity) {
        return entity && entity->m_name == name;
    });
    
    return (it != m_entities.end()) ? it->get() : nullptr;
}

void WorldScene::loadMap(const std::string &filename)
{
    
}

void WorldScene::registerComponent(Component *component)
{
    component->awake(); // Awake вызывается СРАЗУ при создании
    
    if (component->HasFlag(Component::RequiresUpdate)) {
        m_globalUpdatables.push_back(component);
    }
    // Точно так же для физики:
    if (component->HasFlag(Component::RequiresFixedUpdate)) {
        m_globalFixedUpdatables.push_back(component);
    }
    
    m_globalWaitingForStart.push_back(component);
}

void WorldScene::unregisterComponent(Component *component)
{
    // Убираем из очереди Start, если он еще не успел вызваться
    m_globalWaitingForStart.erase(std::remove(m_globalWaitingForStart.begin(), m_globalWaitingForStart.end(), component),
                                  m_globalWaitingForStart.end()
                                  );
    
    m_globalUpdatables.erase(std::remove(m_globalUpdatables.begin(), m_globalUpdatables.end(), component),
                             m_globalUpdatables.end()
                             );
    
    m_globalFixedUpdatables.erase(std::remove(m_globalFixedUpdatables.begin(), m_globalFixedUpdatables.end(), component),
                                  m_globalFixedUpdatables.end()
                                  );
}

void WorldScene::processPendingStarts()
{
    if (m_globalWaitingForStart.empty()) return;
    
    auto processingList = std::move(m_globalWaitingForStart);
    m_globalWaitingForStart.clear();
    
    for (auto* component : processingList)
    {
        if (component->owner->m_isActive && component->enabled)
        {
            component->start();
        }
        else
        {
            m_globalWaitingForStart.push_back(component);
        }
    }
}

void WorldScene::cleanupDeadComponents()
{
    if (m_componentsToDestroy.empty()) return;
    
    for (auto comp : m_componentsToDestroy)
    {
        unregisterComponent(comp);
        comp->owner->freeComponentMemory(comp);
    }
    
    // Очищаем буфер смертников для следующего кадра
    m_componentsToDestroy.clear();
}

void WorldScene::cleanupDeadEntities()
{
    if (m_entitiesToDestroy.empty()) return;
    
    // Делаем копию очереди, так как в процессе удаления родителя
    // могут каскадно вызваться Destroy() для детей, что изменит m_entitiesToDestroy
    auto processingList = std::move(m_entitiesToDestroy);
    m_entitiesToDestroy.clear();
    
    for (Entity* entity : processingList)
    {
        
//        // 1. Обработка иерархии Unreal-style (остается прежней)
//        TransformComponent* transform = entity->getTransform();
//        if (transform)
//        {
//            // Вариант А: Уничтожаем детей вместе с родителем (Каскадное удаление)
//            // Делаем копию вектора детей, так как их Destroy() изменит оригинальный вектор
//            auto children = transform->getChildren();
//            
//            for (TransformComponent* childTransform : children)
//            {
//                childTransform->entity->destroy(); // Вызываем Destroy у дочерней Сущности
//            }
//            
//            // Вариант Б: Если вы НЕ хотите удалять детей, а хотите просто "отвязать" их в мир:
//            // transform->detachAllChildren();
//            
//            // Отвязываем сам удаляемый объект от его родителя
//            transform->setParent(nullptr);
//        }
        
        // 2. Важный момент: Мы НЕ трогаем списки обновлений здесь!
        // Потому что на этом же кадре (чуть раньше или чуть позже) сработает метод
        // CleanupDeadComponents(), который аккуратно вычистит все компоненты
        // этой сущности из m_globalUpdatables, так как они были туда добавлены в Entity::Destroy().
        
        // 3. Физически удаляем только контейнер сущности
        m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
                                        [entity](const std::unique_ptr<Entity>& e) { return e.get() == entity; }),
                         m_entities.end()
                         );
    }
}

void WorldScene::startSimulation(Viewport* viewport)
{
    if (!viewport) return;
    
    m_pAttachedViewport = viewport;
    m_isSimulationMode = true;
    
    // 1. Ищем игровую камеру на сцене
    Camera* gameCamera = nullptr;
    
    for (auto& entity : m_entities) {
        if (!entity->m_isActive) continue;
        
        if (auto cameraComp = entity->getComponent<CameraComponent>()) {
            gameCamera = &cameraComp->m_camera;
            break;
        }
    }
    
    // 2. Если нашли игровую камеру — подменяем её во вьюпорте
    if (gameCamera) {
        m_pAttachedViewport->setCamera(gameCamera);
        // Опционально: отключаем контроллер свободной камеры вьюпорта,
        // чтобы мышь/WASD редактора не двигали игровую камеру
//        m_pAttachedViewport->getCameraController()->setActive(false);
    }
    
    // 3. Запускаем логику симуляции (например, вызов функций Start у скриптов)
    // onSimulationStart();
}

void WorldScene::stopSimulation()
{
    m_isSimulationMode = false;
}
