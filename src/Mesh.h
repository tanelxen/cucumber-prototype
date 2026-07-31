//
//  Cube.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 21.10.24.
//

#pragma once

#include <glm/glm.hpp>
#include <string>

class VertexBuffer;
class VertexArray;
class IndexBuffer;

struct Surface {
    uint32_t MaterialIndex;   // Индекс материала в векторе материалов модели
    uint32_t IndexStart;      // Смещение в индексном буфере (в штуках индексов, НЕ байтах)
    uint32_t IndexCount;      // Сколько индексов рисовать для этой поверхности
    std::string m_name;
};

struct Material {
    glm::vec4 tintColor = glm::vec4(1);
    uint32_t albedoMap = 0;
};

class Mesh
{
public:
    void initCube();
    
    void init(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& texCoords, const std::vector<uint32_t>& indices);
    void init(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<float>& texCoords, const std::vector<uint32_t>& indices);
    
    void setSurfaces(std::vector<Surface>& surfaces) { m_surfaces = surfaces; }
    
    void draw(glm::mat4& transform, glm::vec4 tintColor) const;
    
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
