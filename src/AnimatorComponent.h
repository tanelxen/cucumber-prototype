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

class Skeleton;

class AnimatorComponent: public Component
{
public:
    AnimatorComponent();
    ~AnimatorComponent();
    
    void update(float dt) override;
    
    void setSkeleton(std::shared_ptr<Skeleton> skeleton);
//    void addClip(const AnimationClip& clip) { m_clips.push_back(clip); }
    
    void play(int clipIndex);
    
    const std::vector<glm::mat4>& getFinalBoneMatrices() const { return m_finalBoneMatrices; }
    
    std::vector<std::shared_ptr<AnimationClip>> m_clips;
    int m_currentClipIndex = -1;

private:
    std::shared_ptr<Skeleton> m_skeleton;
    
    float m_currentTime = 0.0f;
    
    // Временные буферы локальных трансформаций костей для текущего кадра
    std::vector<glm::vec3> m_translations;
    std::vector<glm::quat> m_rotations;
    std::vector<glm::vec3> m_scales;
    
    // Финальный массив для отправки в шейдер
    std::vector<glm::mat4> m_finalBoneMatrices;
    std::vector<glm::mat4> m_globalTransforms;
};
