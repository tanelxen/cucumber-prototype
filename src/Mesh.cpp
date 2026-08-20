//
//  Cube.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 21.10.24.
//

#include "Mesh.h"

#include "RenderDevice.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

void Mesh::draw(glm::mat4& transform) const
{
    for (const auto& surface : m_surfaces)
    {
        auto& material = m_materials[surface.materialSlot];
        
        RenderCommand cmd = {
            .vao = vertexArray->getID(),
            .start = surface.startIndex,
            .count = surface.count,
            
            .pipeline = PipelineType::Skinned,
            .blendMode = material.blendMode,
            .baseTexture = material.albedoMap,
            .color = material.tintColor,
            
            .transform = transform,
            .bones = &m_finalBoneMatrices
        };
        
        RenderDevice::submit(cmd);
    }
}

void Mesh::setPositions(const std::vector<glm::vec3> &positions)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    posBuffer = new VertexBuffer(positions.data(), sizeof(glm::vec3) * (uint32_t)positions.size());
    posBuffer->setLayout({ { VertexAttributeLocation::Position, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(posBuffer);
}

void Mesh::setNormals(const std::vector<glm::vec3> &normals)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    nrmBuffer = new VertexBuffer(normals.data(), sizeof(glm::vec3) * (uint32_t)normals.size());
    nrmBuffer->setLayout({ { VertexAttributeLocation::Normal, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(nrmBuffer);
}

void Mesh::setTexCoords(const std::vector<glm::vec2> &uv)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    texBuffer = new VertexBuffer(uv.data(), sizeof(glm::vec2) * (uint32_t)uv.size());
    texBuffer->setLayout({ { VertexAttributeLocation::TexCoord, ShaderDataType::Float2 } });
    vertexArray->setVertexBuffer(texBuffer);
}

void Mesh::setIndices(const std::vector<uint32_t> &indices)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    indexBuffer = new IndexBuffer(indices.data(), (uint32_t)indices.size());
    vertexArray->setIndexBuffer(indexBuffer);
}

void Mesh::setSkinned(const std::vector<glm::ivec4> &boneIndices, const std::vector<glm::vec4> &weights)
{
    bonesBuffer = new VertexBuffer(boneIndices.data(), sizeof(glm::ivec4) * (uint32_t)boneIndices.size());
    bonesBuffer->setLayout({ { VertexAttributeLocation::BoneIndices, ShaderDataType::Int4 } });
    vertexArray->setVertexBuffer(bonesBuffer);
    
    weightsBuffer = new VertexBuffer(weights.data(), sizeof(glm::vec4) * (uint32_t)weights.size());
    weightsBuffer->setLayout({ { VertexAttributeLocation::BoneWeights, ShaderDataType::Float4 } });
    vertexArray->setVertexBuffer(weightsBuffer);
}

void Mesh::update(float dt)
{
    // Если скелета или анимаций нет, прерываем выполнение
    if (m_skeleton.empty() || m_animations.empty()) return;
    
    // Получаем текущий клип
    AnimationClip& clip = m_animations[m_currentAnimIndex];
    
    // Обновляем и зацикливаем время
    if (clip.duration > 0.0f) {
        m_animationTime += dt;
        m_animationTime = fmod(m_animationTime, clip.duration);
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
    for (const auto& track : clip.tracks)
    {
        Bone& bone = m_skeleton[track.boneIndex];
        int p0, p1;
        float factor;
        
        // --- Интерполяция позиции ---
        if (!track.positionTimes.empty()) {
            if (track.positionTimes.size() == 1) {
                bone.translation = track.positions[0];
            } else {
                getFramesAndFactor(track.positionTimes, m_animationTime, p0, p1, factor);
                bone.translation = glm::mix(track.positions[p0], track.positions[p1], factor);
            }
        }
        
        // --- Интерполяция вращения ---
        if (!track.rotationTimes.empty()) {
            if (track.rotationTimes.size() == 1) {
                bone.rotation = track.rotations[0];
            } else {
                getFramesAndFactor(track.rotationTimes, m_animationTime, p0, p1, factor);
                // Для кватернионов используем сферическую линейную интерполяцию (slerp) и нормализуем результат
                bone.rotation = glm::normalize(glm::slerp(track.rotations[p0], track.rotations[p1], factor));
            }
        }
        
        // --- Интерполяция масштаба ---
        if (!track.scaleTimes.empty()) {
            if (track.scaleTimes.size() == 1) {
                bone.scale = track.scales[0];
            } else {
                getFramesAndFactor(track.scaleTimes, m_animationTime, p0, p1, factor);
                bone.scale = glm::mix(track.scales[p0], track.scales[p1], factor);
            }
        }
    }
    
    // 2. Вычисляем глобальные матрицы
    // Мы проходим по массиву костей последовательно. Формат glTF обычно гарантирует,
    // что узел-родитель предшествует узлу-потомку в массиве (топологическая сортировка).
    for (size_t i = 0; i < m_skeleton.size(); ++i)
    {
        Bone& bone = m_skeleton[i];
        
        // Собираем локальную матрицу (T * R * S)
        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), bone.translation) *
        glm::mat4_cast(bone.rotation) *
        glm::scale(glm::mat4(1.0f), bone.scale);
        
        // Умножаем на глобальную матрицу родителя, если он есть
        if (bone.parentIndex >= 0) {
            bone.globalTransform = m_skeleton[bone.parentIndex].globalTransform * localTransform;
        } else {
            bone.globalTransform = localTransform;
        }
        
        // 3. Вычисляем финальную матрицу для отправки в шейдер (Global * InverseBind)
        m_finalBoneMatrices[i] = bone.globalTransform * bone.inverseBindMatrix;
    }
}


