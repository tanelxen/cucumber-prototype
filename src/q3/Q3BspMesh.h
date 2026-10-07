//
//  BspScene.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 27.10.24.
//

#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "RenderDevice.h"
#include "Mesh.h"

#define MAX_TEXTURES 1000

class VertexArray;
class VertexBuffer;
class IndexBuffer;
class Surface;


class Q3BSPAsset;

class Q3BspMesh
{
public:
    Q3BspMesh();
    ~Q3BspMesh();
    
    void initFromBsp(Q3BSPAsset* bsp);
    void draw(RenderView& view);
    
private:
    void initBuffers();
    void GenerateTexture();
    
//    unsigned int m_vao;
//    unsigned int m_vbo;
//    unsigned int m_ibo;
    
    Material m_textures[MAX_TEXTURES];
    unsigned int m_lightmap;

    unsigned int m_missingTexId;
    
    std::vector<Surface> m_lightmapped;
    std::vector<Surface> m_vertexlit;
    
    
    std::unique_ptr<VertexArray> m_vao;
    std::unique_ptr<VertexBuffer> m_vertexBuffer;
    std::unique_ptr<IndexBuffer>  m_indexBuffer;
};
