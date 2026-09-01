//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <typeindex>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "../vendor/json.hpp"

#include "AudioSystem.h"

//struct Transform
//{
//    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
//    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
//    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
//    
//    glm::mat4 toMat4();
//};

class Entity;

class Component {
public:
    Entity* owner = nullptr;
    bool enabled = true;
    
    virtual ~Component() = default;
    virtual void awake() {}
    virtual void update(float dt) {}
    
    virtual void save(nlohmann::json& j) const = 0;
    virtual void load(const nlohmann::json& j) = 0;
    virtual const std::string getTypeName() const = 0;
    
    virtual std::type_index GetTypeIndex() const {
        return std::type_index(typeid(*this));
    }
};

struct TransformComponent : Component
{
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    
    glm::mat4 toMat4();
    
    const std::string getTypeName() const override { return "TransformComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale",    {scale.x,    scale.y,    scale.z}}
        };
    }
    
    void load(const nlohmann::json& j) override
    {
        auto pos = j.at("position").get<std::vector<float>>();
        position = glm::vec3(pos[0], pos[1], pos[2]);
        
        auto rot = j.at("rotation").get<std::vector<float>>();
        rotation = glm::vec3(rot[0], rot[1], rot[2]);
        
        auto scl = j.at("scale").get<std::vector<float>>();
        scale = glm::vec3(scl[0], scl[1], scl[2]);
    }
};

struct MeshComponent : Component
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

struct AudioComponent : public Component
{
    std::string path = "<no sound>";
    
    ma_sound m_sound;
    bool m_isInitialized = false;
    float m_length;
    
    const std::string getTypeName() const override { return "AudioComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json{ {"source", path} };
    }
    
    void load(const nlohmann::json& j) override
    {
        path = j.at("source");
    }
    
    void init(const std::string& filePath)
    {
        destroy(); // Очищаем старый звук, если он был
        
        ma_uint32 flags = 0;
        
        ma_result result = ma_sound_init_from_file(
                                                   AudioSystem::Get().GetEngine(),
                                                   filePath.c_str(),
                                                   flags,
                                                   nullptr, // Группа микширования (опционально)
                                                   nullptr, // Процесс уведомлений (опционально)
                                                   &m_sound
                                                   );
        
        if (result != MA_SUCCESS) {
            std::cerr << "Failed to load sound: " << filePath << std::endl;
            return;
        }
        
        m_isInitialized = true;
        
        m_length = GetLength();
    }
    
    void update(float dt) override;
    
    void play()
    {
        if (!m_isInitialized) return;
        ma_sound_start(&m_sound);
    }
    
    void stop() {
        if (!m_isInitialized) return;
        ma_sound_stop(&m_sound);
    }
    
    void setLooping(bool loop) {
        if (!m_isInitialized) return;
        ma_sound_set_looping(&m_sound, loop ? MA_TRUE : MA_FALSE);
    }
    
    void setVolume(float volume) { // volume от 0.0f до 1.0f+
        if (!m_isInitialized) return;
        ma_sound_set_volume(&m_sound, volume);
    }
    
    void destroy()
    {
        if (m_isInitialized) {
            ma_sound_uninit(&m_sound);
            m_isInitialized = false;
        }
    }
    
    float GetCurrentTime() const {
        if (!m_isInitialized) return 0.0f;
        
        // 1. Получаем абсолютное время проигрывания
        ma_uint64 absoluteFrames = ma_sound_get_time_in_pcm_frames(&m_sound);
        ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
        float absoluteTimeInSeconds = (float)absoluteFrames / (float)sampleRate;
        
        // 2. Получаем длину трека
        float totalLength = m_length;
        if (totalLength <= 0.0f) return 0.0f;
        
        // 3. Если включен лупинг, зацикливаем время математически
        if (IsLooping()) {
            return std::fmodf(absoluteTimeInSeconds, totalLength);
        }
        
        // Если лупинг выключен, miniaudio останавливает трек на его максимальной длине
        return absoluteTimeInSeconds > totalLength ? totalLength : absoluteTimeInSeconds;
    }
    
    // Возвращает общую длину трека в секундах
    float GetLength() const {
        if (!m_isInitialized) return 0.0f;
        ma_uint64 length = 0;
        ma_sound_get_length_in_pcm_frames(const_cast<ma_sound*>(&m_sound), &length);
        
        ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
        return (float)length / (float)sampleRate;
    }
    
    // Позволяет перематывать трек (нужно для прогресс-бара, если делать его интерактивным)
    void SetCurrentTime(float seconds) {
        if (!m_isInitialized) return;
        ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
        ma_uint64 frameIndex = (ma_uint64)(seconds * sampleRate);
        ma_sound_seek_to_pcm_frame(&m_sound, frameIndex);
    }
    
    bool IsPlaying() const {
        if (!m_isInitialized) return false;
        return ma_sound_is_playing(&m_sound) == MA_TRUE;
    }
    
    // Метод для получения текущих настроек (нужен для ImGui)
    float GetVolume() const { return m_isInitialized ? ma_sound_get_volume(&m_sound) : 0.0f; }
    bool IsLooping() const { return m_isInitialized ? (ma_sound_is_looping(&m_sound) == MA_TRUE) : false; }

};

class Entity
{
public:
    Entity(const std::string &name);
    
    std::string m_name;
    bool m_isActive;
    
//    const std::string& getName() const { return m_name; }
    const std::vector<std::unique_ptr<Component>>& getComponents() const { return m_components; }
    
    // Добавление компонента
    template<typename T, typename... TArgs>
    T* addComponent(TArgs&&... args)
    {
        auto newComponent = std::make_unique<T>(std::forward<TArgs>(args)...);
        T* componentPtr = newComponent.get();
        
        componentPtr->owner = this;
        m_components.push_back(std::move(newComponent));
        m_componentMap[typeid(T)] = componentPtr;
        
        componentPtr->awake();
        return componentPtr;
    }
    
    // Получение компонента
    template<typename T>
    T* getComponent()
    {
        auto it = m_componentMap.find(typeid(T));
        return (it != m_componentMap.end()) ? static_cast<T*>(it->second) : nullptr;
    }
    
    // Обновление всех компонентов сущности
    void update(float dt)
    {
        for (auto& component : m_components) {
            if (component->enabled) {
                component->update(dt);
            }
        }
    }
    
    const std::vector<std::unique_ptr<Component>>& getAllComponents() const { return m_components; }
    
private:
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_map<std::type_index, Component*> m_componentMap;
};
