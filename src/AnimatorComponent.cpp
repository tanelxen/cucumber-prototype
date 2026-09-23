//
//  AnimatorComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.08.26.
//

#include "AnimatorComponent.h"
#include "Skeleton.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>

AnimatorComponent::AnimatorComponent() : Component(Flags::RequiresUpdate)
{
    
}

AnimatorComponent::~AnimatorComponent()
{
    // ВАЖНО: Освобождаем функцию, если компонент удаляется
    if (m_onClipFinishedCallback) {
        m_onClipFinishedCallback->Release();
    }
}

void AnimatorComponent::setSkeleton(std::shared_ptr<Skeleton> skeleton)
{
    m_skeleton = skeleton;
    
    if (m_skeleton)
    {
        size_t count = m_skeleton->bones.size();
        
        m_finalBoneMatrices.resize(count, glm::mat4(1.0f));
        m_globalTransforms.resize(count, glm::mat4(1.0f));
        
        m_translations.resize(count);
        m_rotations.resize(count);
        m_scales.resize(count);
        
        // Заполняем локальные буферы дефолтными трансформациями из glTF
        for (size_t i = 0; i < count; ++i)
        {
            m_translations[i] = m_skeleton->bones[i].defaultTranslation;
            m_rotations[i]    = m_skeleton->bones[i].defaultRotation;
            m_scales[i]       = m_skeleton->bones[i].defaultScale;
        }
        
        // Сразу принудительно рассчитываем матрицы скиннинга для T-позы/Rest позы
        for (size_t i = 0; i < count; ++i) {
            const auto& bone = m_skeleton->bones[i];
            
            glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), m_translations[i]) *
            glm::mat4_cast(m_rotations[i]) *
            glm::scale(glm::mat4(1.0f), m_scales[i]);
            
            if (bone.parentIndex >= 0) {
                m_globalTransforms[i] = m_globalTransforms[bone.parentIndex] * localTransform;
            } else {
                m_globalTransforms[i] = localTransform;
            }
            
            m_finalBoneMatrices[i] = m_globalTransforms[i] * bone.inverseBindMatrix;
        }
    }
}

int AnimatorComponent::findClipIndex(const std::string& name) const
{
    for (size_t i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i]->name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AnimatorComponent::play(int clipIndex, bool once)
{
    if (clipIndex >= 0 && clipIndex < m_clips.size())
    {
        m_currentClipIndex = clipIndex;
        m_currentTime = 0.0f;
        m_isStopped = false;
        m_isOnce = once;
        
        // При принудительном вызове play сбрасываем очередь
        m_nextClipIndex = -1;
    }
}

void AnimatorComponent::play(const std::string& clipName, bool once)
{
    int index = findClipIndex(clipName);
    play(index, once);
}

void AnimatorComponent::playQueue(int currentClipIndex, int nextClipIndex, bool nextOnce)
{
    if (currentClipIndex >= 0 && currentClipIndex < m_clips.size()) {
        play(currentClipIndex, true); // Первый клип всегда играет один раз
        m_nextClipIndex = nextClipIndex;
        m_nextClipOnce = nextOnce;
    }
}

void AnimatorComponent::playQueue(const std::string& currentClipName, const std::string& nextClipName, bool nextOnce)
{
    int currentIdx = findClipIndex(currentClipName);
    int nextIdx = findClipIndex(nextClipName);
    playQueue(currentIdx, nextIdx, nextOnce);
}

void AnimatorComponent::update(float dt)
{
    if (m_isStopped || !m_skeleton || m_currentClipIndex < 0 || m_clips.empty()) return;
    
    auto clip = m_clips[m_currentClipIndex];
    
    if (clip->duration > 0.0f)
    {
        // Проверяем, завершится ли анимация на этом кадре
        if (m_isOnce && (m_currentTime + dt) >= clip->duration)
        {
            m_currentTime = clip->duration; // Выставляем финальный кадр
            std::string finishedClipName = clip->name; // Запоминаем имя до переключения
            
            // Проверяем, есть ли что-то в очереди (Вариант Б)
            if (m_nextClipIndex >= 0 && m_nextClipIndex < m_clips.size())
            {
                int nextIdx = m_nextClipIndex;
                bool nextOnce = m_nextClipOnce;
                
                // Сбрасываем очередь перед запуском, чтобы избежать зацикливания
                m_nextClipIndex = -1;
                
                // Запускаем следующую анимацию
                play(nextIdx, nextOnce);
            }
            else
            {
                // Если очереди нет, просто останавливаемся
                m_isStopped = true;
            }
            
            // ВЫЗОВ КОЛЛБЭКА
            if (m_onClipFinishedCallback)
            {
                asIScriptEngine* engine = m_onClipFinishedCallback->GetEngine();
                
                // Получаем активный контекст скрипта (или запрашиваем новый, если его нет) [1]
                asIScriptContext* ctx = asGetActiveContext();
                bool isNested = true;
                if (!ctx) {
                    ctx = engine->RequestContext();
                    isNested = false;
                }
                
                // Подготавливаем и выполняем вызов функции
                ctx->Prepare(m_onClipFinishedCallback);
                ctx->SetArgAddress(0, &finishedClipName); // Передаем строку как первый аргумент [1]
                
                int r = ctx->Execute();
                if (r != asEXECUTION_FINISHED) {
                    std::cerr << "[Animator] Ошибка выполнения коллбэка в скрипте!" << std::endl;
                }
                
                // Если контекст брали из пула — возвращаем его
                if (!isNested) {
                    engine->ReturnContext(ctx);
                }
            }
        }
        else
        {
            m_currentTime = fmod(m_currentTime + dt, clip->duration);
        }
    }

    
    // Вспомогательная функция для поиска двух соседних кадров и фактора интерполяции
    auto getFramesAndFactor = [](const std::vector<float>& times, float animTime, int& p0, int& p1, float& factor) {
        p0 = 0; p1 = 0; factor = 0.0f;
        for (size_t i = 0; i < times.size() - 1; ++i) {
            if (animTime < times[i + 1]) {
                p0 = static_cast<int>(i);
                p1 = static_cast<int>(i + 1);
                factor = (animTime - times[p0]) / (times[p1] - times[p0]);
                factor = glm::clamp(factor, 0.0f, 1.0f); // Защита от выхода за пределы
                return;
            }
        }
    };
    
    // 1. Применяем анимацию к локальным трансформациям костей
    for (const auto& track : clip->tracks)
    {
        int bIdx = track.boneIndex;
        auto& translation = m_translations[bIdx];
        auto& rotation = m_rotations[bIdx];
        auto& scale = m_scales[bIdx];
        
        int p0, p1;
        float factor;
        
        // --- Интерполяция позиции ---
        if (!track.positionTimes.empty())
        {
            if (track.positionTimes.size() == 1) {
                translation = track.positions[0];
            } else {
                getFramesAndFactor(track.positionTimes, m_currentTime, p0, p1, factor);
                translation = glm::mix(track.positions[p0], track.positions[p1], factor);
            }
        }
        
        // --- Интерполяция вращения ---
        if (!track.rotationTimes.empty()) {
            if (track.rotationTimes.size() == 1) {
                rotation = track.rotations[0];
            } else {
                getFramesAndFactor(track.rotationTimes, m_currentTime, p0, p1, factor);
                // Для кватернионов используем сферическую линейную интерполяцию (slerp) и нормализуем результат
                rotation = glm::slerp(track.rotations[p0], track.rotations[p1], factor);
            }
        }
        
        // --- Интерполяция масштаба ---
        if (!track.scaleTimes.empty()) {
            if (track.scaleTimes.size() == 1) {
                scale = track.scales[0];
            } else {
                getFramesAndFactor(track.scaleTimes, m_currentTime, p0, p1, factor);
                scale = glm::mix(track.scales[p0], track.scales[p1], factor);
            }
        }
    }
    
    // 3. Расчет глобальных матриц и финальных матриц скиннинга
    for (size_t i = 0; i < m_skeleton->bones.size(); ++i) {
        const auto& bone = m_skeleton->bones[i];
        
        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), m_translations[i]) *
        glm::mat4_cast(m_rotations[i]) *
        glm::scale(glm::mat4(1.0f), m_scales[i]);
        
        if (bone.parentIndex >= 0) {
            m_globalTransforms[i] = m_globalTransforms[bone.parentIndex] * localTransform;
        } else {
            m_globalTransforms[i] = localTransform;
        }
        
        m_finalBoneMatrices[i] = m_globalTransforms[i] * bone.inverseBindMatrix;
    }
}

void AnimatorComponent::setClipFinishedCallback(asIScriptFunction* func)
{
    // Если уже был привязан коллбэк — освобождаем его
    if (m_onClipFinishedCallback) {
        m_onClipFinishedCallback->Release();
        m_onClipFinishedCallback = nullptr;
    }
    
    if (func) {
        m_onClipFinishedCallback = func;
        m_onClipFinishedCallback->AddRef(); // Заставляем скрипт держать её в памяти [1]
    }
}
