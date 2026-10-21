//
//  MapMesh.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#pragma once

#include "RenderDevice.h"
#include "Mesh.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

struct MapAsset;

class MapMesh
{
public:
    MapMesh();
    ~MapMesh();
    
    void buildFromMapAsset(const MapAsset& mapData);
    void draw(RenderView& view) const;
    
    std::string m_name;
    
public:
    std::vector<Surface> m_surfaces;
    std::vector<Material> m_materials;
    
private:
    std::unique_ptr<VertexArray>  m_vao;
    std::unique_ptr<VertexBuffer> m_posBuffer;
    std::unique_ptr<VertexBuffer> m_shadingBuffer;
    std::unique_ptr<IndexBuffer>  m_indexBuffer;
};
