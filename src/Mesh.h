//
//  Cube.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 21.10.24.
//

#pragma once

#include "RenderDevice.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>

class VertexBuffer;
class VertexArray;
class IndexBuffer;

struct Surface
{
    std::string m_name;
    uint32_t materialSlot;
    uint32_t startIndex;
    uint32_t count;
};

struct Material
{
    glm::vec4 tintColor = glm::vec4(1);
    uint32_t albedoMap = 0;
    
    BlendMode blendMode = BlendMode::Opaque;
    
    std::string m_name;
    std::string m_source;
    bool isEmbedded;
};

struct Bone {
    int parentIndex = -1;
    glm::mat4 inverseBindMatrix{1.0f}; // Матрица перехода из локального пространства меша в пространство кости
    
    // Текущие трансформы
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    
    glm::mat4 globalTransform{1.0f};
};

// Хранит ключи для конкретной кости
struct AnimationTrack {
    int boneIndex;
    
    std::vector<float> positionTimes;
    std::vector<glm::vec3> positions;
    
    std::vector<float> rotationTimes;
    std::vector<glm::quat> rotations;
    
    std::vector<float> scaleTimes;
    std::vector<glm::vec3> scales;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationTrack> tracks;
};

class Mesh
{
public:
    void setPositions(const std::vector<glm::vec3>& positions);
    void setNormals(const std::vector<glm::vec3>& normals);
    void setTexCoords(const std::vector<glm::vec2>& uv);
    void setIndices(const std::vector<uint32_t>& indices);
    
    void setSkinned(const std::vector<glm::ivec4>& boneIndices, const std::vector<glm::vec4>& weights);
    
    void setSurfaces(std::vector<Surface>& surfaces) { m_surfaces = surfaces; }
    
    void update(float dt);
    void draw(glm::mat4& transform) const;
    
    std::string m_name;
    std::vector<Surface> m_surfaces;
    std::vector<Material> m_materials;
    
    std::vector<Bone> m_skeleton;
    std::vector<AnimationClip> m_animations;
    std::vector<glm::mat4> m_finalBoneMatrices; // То, что отправим в шейдер
    
private:
    VertexArray* vertexArray;
    
    VertexBuffer* posBuffer;
    VertexBuffer* nrmBuffer;
    VertexBuffer* texBuffer;
    
    VertexBuffer* bonesBuffer;
    VertexBuffer* weightsBuffer;
    
    IndexBuffer* indexBuffer;
    
    float m_animationTime = 0.0f;
    int m_currentAnimIndex = 0;
};
