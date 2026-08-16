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

//struct MaterialAsset
//{
//    
//};

class Mesh
{
public:
    void initCube();
    
    void init(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& texCoords, const std::vector<uint32_t>& indices);
    void init(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<float>& texCoords, const std::vector<uint32_t>& indices);
    
    void setSurfaces(std::vector<Surface>& surfaces) { m_surfaces = surfaces; }
    
    void draw(glm::mat4& transform) const;
    
    std::string m_name;
    std::vector<Surface> m_surfaces;
    std::vector<Material> m_materials;
    
private:
    VertexBuffer* posBuffer;
    VertexBuffer* nrmBuffer;
    VertexBuffer* texBuffer;
    IndexBuffer* indexBuffer;
    VertexArray* vertexArray;
    
    
};
