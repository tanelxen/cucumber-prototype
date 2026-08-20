//
//  Cube.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 21.10.24.
//

#pragma once

#include "RenderDevice.h"

#include <glm/glm.hpp>
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
    
private:
    VertexArray* vertexArray;
    
    VertexBuffer* posBuffer;
    VertexBuffer* nrmBuffer;
    VertexBuffer* texBuffer;
    
    VertexBuffer* bonesBuffer;
    VertexBuffer* weightsBuffer;
    
    IndexBuffer* indexBuffer;
};
