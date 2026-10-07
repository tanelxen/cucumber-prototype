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
#include <glm/gtc/type_precision.hpp>

#include <string>

class VertexArray;
class VertexBuffer;
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

struct VertexPosition {
    glm::vec3 position;
};

struct VertexShading {
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct VertexSkinning {
    glm::u8vec4 boneIndices;
    glm::u8vec4 boneWeights;
};

class Mesh
{
public:
    Mesh();
    ~Mesh();
    
    void setVertexData(const std::vector<VertexPosition>& positions, const std::vector<VertexShading>& shadingData, const std::vector<VertexSkinning>* skinningData);
    void setIndices(const std::vector<uint32_t>& indices);
    void setSurfaces(std::vector<Surface>& surfaces) { m_surfaces = surfaces; }
    
    void draw(RenderView& view, glm::mat4& transform) const;
    void draw(RenderView& view, glm::mat4& transform, const std::vector<glm::mat4>& bonesPalette) const;
    
    std::string m_name;
    std::vector<Surface> m_surfaces;
    std::vector<Material> m_materials;
    
    bool m_useAmbientLight = true;
    
private:
    std::unique_ptr<VertexArray> m_vao;
    
    std::unique_ptr<VertexBuffer> m_posBuffer;
    std::unique_ptr<VertexBuffer> m_shadingBuffer;
    std::unique_ptr<VertexBuffer> m_skinningBuffer;
    
    std::unique_ptr<IndexBuffer>  m_indexBuffer;
};
