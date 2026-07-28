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

class Mesh
{
public:
    void initCube();
    
    void init(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<uint32_t>& indices);
    void init(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<uint32_t>& indices);
    
    void draw() const;
    
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec4 color;
    
    std::string m_name;
    
private:
    VertexBuffer* posBuffer;
    VertexBuffer* nrmBuffer;
    IndexBuffer* indexBuffer;
    VertexArray* vertexArray;
};
