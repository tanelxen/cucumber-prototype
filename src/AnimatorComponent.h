//
//  AnimatorComponent.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.08.26.
//

#pragma once

#include "Animation.h"
#include "Entity.h"

#include <glm/gtc/quaternion.hpp>
#include <memory>

#include <angelscript.h> // Нужен доступ к типам AngelScript

class Skeleton;

class asIScriptFunction;

// Событие которое мы млжем добавить из скриптов
struct AnimationNotify
{
    float time;                       // Время триггера (например, 0.44f)
    asIScriptFunction* callback = nullptr; // Какую функцию скрипта вызвать
    bool hasFired = false;            // Флаг-предохранитель
};

class AnimatorComponent: public Component
{
public:
    AnimatorComponent();
    ~AnimatorComponent();
    
    void setSkeleton(std::shared_ptr<Skeleton> skeleton);
    
    // Обычное проигрывание
    void play(const std::string& clipName, bool once = false, float blendTime = 0.0f);
    void play(int clipIndex, bool once = false, float blendTime = 0.0f);
    
    // Метод для проигрывания очереди
    void playQueue(const std::string& currentClipName, const std::string& nextClipName, bool nextOnce = false, float blendTime = 0.2f);
    void playQueue(int currentClipIndex, int nextClipIndex, bool nextOnce = false, float blendTime = 0.2f);
    
    // Вспомогательный метод для поиска индекса клипа по имени
    int findClipIndex(const std::string& name) const;
    
    const std::vector<glm::mat4>& getFinalBoneMatrices() const { return m_finalBoneMatrices; }
    
    
    // МЕТОД ДЛЯ СКРИПТА (Аналог Unreal #exec MESH NOTIFY)
    void addNotify(const std::string& clipName, float time, asIScriptFunction* callback);
    
    // Очистить все нотифаи для конкретного клипа (если нужно сбросить в рантайме)
    void clearNotifies(const std::string& clipName);
    
public:
    void update(float dt) override;
    
    const std::string getTypeName() const override { return "AnimatorComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json{ {"source_glb", path} };
        
        if (m_currentClipIndex > -1)
        {
            auto clip = m_clips[m_currentClipIndex];
            j = nlohmann::json{ {"clip", clip->name} };
        }
        else
        {
            j = nlohmann::json{ {"clip", "<none>"} };
        }
    }
    
    void load(const nlohmann::json& j) override
    {
        std::string clipName = j.at("clip");
        
        auto it = std::find_if(m_clips.begin(), m_clips.end(), [&clipName](const std::shared_ptr<AnimationClip>& clip) {
            return clip->name == clipName;
        });
        
//        if (it != m_clips.end())
//        {
//            int index = static_cast<int>(it - m_clips.begin());
//            play(index);
//        }
    }
    
public:
    std::vector<std::shared_ptr<AnimationClip>> m_clips;
    int m_currentClipIndex = -1;
    
    // Путь к модели из которой взяты анимации
    std::string path;

private:
    // СТРУКТУРА ДЛЯ ХРАНЕНИЯ СНИМКА ПОЗЫ (Rest/Source Pose во время блендинга)
    struct BlendPose {
        std::vector<glm::vec3> translations;
        std::vector<glm::quat> rotations;
        std::vector<glm::vec3> scales;
    };
    
    std::shared_ptr<Skeleton> m_skeleton;
    
    float m_currentTime = 0.0f;
    bool m_isStopped = true;
    bool m_isOnce = false;
    
    // Переменные для Очереди (Вариант Б)
    int m_nextClipIndex = -1;
    bool m_nextClipOnce = false;
    float m_nextClipBlendTime = 0.2f;
    
    // ПЕРЕМЕННЫЕ ДЛЯ БЛЕНДИНГА
    bool m_isBlending = false;
    float m_blendTimeTotal = 0.0f;
    float m_blendTimeCurrent = 0.0f;
    BlendPose m_sourcePose; // Сюда зафиксируем позу костей в момент клика play()
    
    // Твои массивы трансформаций
    std::vector<glm::mat4> m_finalBoneMatrices;
    std::vector<glm::mat4> m_globalTransforms;
    std::vector<glm::vec3> m_translations;
    std::vector<glm::quat> m_rotations;
    std::vector<glm::vec3> m_scales;
    
public:
    // Храним указатель на скриптовую функцию-коллбэк
    asIScriptFunction* m_onClipFinishedCallback = nullptr;
    asIScriptFunction* onAnimEvent = nullptr;
    
private:
    // Будем сбрасывать флаги событий при каждом запуске анимации
    void resetEvents(AnimationClip* clip);
    
    // Чтобы не портить исходный shared_ptr ресурса AnimationClip (который шарится между всеми врагами),
    // мы будем хранить нотифаи прямо в компоненте, привязав их к индексу клипа.
    // int -> индекс клипа, vector -> список его кастомных нотифаев
    std::unordered_map<int, std::vector<AnimationNotify>> m_customNotifies;
    void resetNotifies(int clipIndex);
};
