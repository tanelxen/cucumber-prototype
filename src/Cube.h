//
//  Cube.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 21.10.24.
//

#pragma once

#include "Shader.h"

#include <glm/glm.hpp>
#include <string>

class Camera;
class VertexBuffer;
class VertexArray;
class IndexBuffer;

//class Cube
//{
//    unsigned int vbo;
//    unsigned int ibo;
//    unsigned int vao;
//    Shader shader;
//    
//public:
//    void init();
//    void draw(const Camera& camera) const;
//    
//    glm::vec3 position;
//    glm::vec3 scale;
//    glm::vec4 color;
//};
//
//class WiredCube
//{
//    unsigned int vbo;
//    unsigned int ibo;
//    unsigned int vao;
//    Shader shader;
//    
//public:
//    void init();
//    void draw(const Camera& camera) const;
//    
//    glm::vec3 position;
//    glm::vec3 scale;
//    float yaw;
//    
//    glm::vec4 color;
//};

class ShadedCube
{
public:
    void init();
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
