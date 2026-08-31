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

AnimatorComponent::AnimatorComponent() = default;
AnimatorComponent::~AnimatorComponent() = default;

void AnimatorComponent::setSkeleton(std::shared_ptr<Skeleton> skeleton)
{
    m_skeleton = skeleton;
    
    if (m_skeleton)
    {
        size_t count = m_skeleton->bones.size();
        
        m_finalBoneMatrices.resize(count, glm::mat4(1.0f));
        m_globalTransforms.resize(count, glm::mat4(1.0f));
        m_translations.resize(count, glm::vec3(0.0f));
        m_rotations.resize(count, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        m_scales.resize(count, glm::vec3(1.0f));
    }
}

void AnimatorComponent::play(int clipIndex)
{
    if (clipIndex >= 0 && clipIndex < m_clips.size())
    {
        m_currentClipIndex = clipIndex;
        m_currentTime = 0.0f;
    }
}

void AnimatorComponent::update(float dt)
{
    if (!m_skeleton || m_currentClipIndex < 0 || m_clips.empty()) return;

    auto clip = m_clips[m_currentClipIndex];
    
    if (clip->duration > 0.0f)
    {
        m_currentTime = fmod(m_currentTime + dt, clip->duration);
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
