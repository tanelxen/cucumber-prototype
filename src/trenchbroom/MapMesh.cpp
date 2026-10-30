//
//  MapMesh.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//
//  Геометрия карты нарезана на поверхности "ячейка сетки x текстура".
//  У каждой поверхности свой AABB. Отсечением и построением команд MapMesh не
//  занимается: поверхности отдаются в RenderScene как статические объекты
//  (SceneExtractor).
//

#include "MapMesh.h"

#include "GLState.h"
#include "IndexBuffer.h"
#include "MapAsset.h"
#include "RenderDevice.h"
#include "TextureManager.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "Q3Shaders.h"

#include <glad/glad.h>

#include <iostream>
#include <string>
#include <unordered_map>

namespace
{
Quake3Shaders s_q3shaders;
uint64_t s_nextRevision = 1;
}

MapMesh::MapMesh() = default;

MapMesh::~MapMesh()
{
    if (m_lightmapTexture != 0)
    {
        const GLuint texture = m_lightmapTexture;
        glDeleteTextures(1, &texture);
    }
}

unsigned int MapMesh::vaoId() const
{
    return m_vao ? m_vao->getID() : 0;
}

bool MapMesh::buildFromMapAsset(const MapAsset& mapData, const LightmapUnwrapSettings& unwrapSettings)
{
    m_vao.reset();
    m_posBuffer.reset();
    m_shadingBuffer.reset();
    m_lightmapBuffer.reset();
    m_indexBuffer.reset();
    m_surfaces.clear();
    m_materials.clear();
    m_surfaceBounds.clear();
    m_positions.clear();
    m_normals.clear();
    m_indices.clear();
    m_lightmapTexCoords.clear();
    m_lightmapAtlasSize = 0;

    if (m_lightmapTexture != 0)
    {
        const GLuint texture = m_lightmapTexture;
        glDeleteTextures(1, &texture);
        m_lightmapTexture = 0;
    }

    m_revision = s_nextRevision++;
    if (mapData.entities.empty())
    {
        std::cerr << "[MapMesh] Map has no worldspawn entity" << std::endl;
        return false;
    }

    const auto scriptsPath = mapData.m_basepath / "scripts";
    s_q3shaders.initFromDir(scriptsPath.string().c_str());

    struct TextureInfo
    {
        int width = 256;
        int height = 256;
        RenderDevice::handle_t handle = 0;
    };
    std::unordered_map<std::string, TextureInfo> textureCache;
    const auto getTexture = [&](const std::string& name) -> const TextureInfo&
    {
        auto it = textureCache.find(name);
        if (it != textureCache.end()) return it->second;

        TextureInfo info;
        auto textureAsset = TextureManager::Get().loadTexture("wolf/textures/" + name);
        if (textureAsset)
        {
            info.width = textureAsset->getWidth();
            info.height = textureAsset->getHeight();
            info.handle = textureAsset->getHandle();
        }
        return textureCache.emplace(name, info).first->second;
    };

    MapMeshBuildResult build;
    std::string error;
    const auto resolveDimensions = [&](const std::string& name) {
        const auto& texture = getTexture(name);
        return glm::ivec2(texture.width, texture.height);
    };
    if (!MapMeshBuilder::build(mapData, unwrapSettings, resolveDimensions, build, error))
    {
        std::cerr << "[MapMesh] Failed to build map mesh: " << error << std::endl;
        return false;
    }

    std::unordered_map<std::string, uint32_t> materialSlotByTexture;
    m_surfaces.reserve(build.surfaces.size());
    m_surfaceBounds.reserve(build.surfaces.size());
    for (const auto& buildSurface : build.surfaces)
    {
        auto slotIt = materialSlotByTexture.find(buildSurface.materialName);
        if (slotIt == materialSlotByTexture.end())
        {
            std::string mapParam = buildSurface.materialName;
            BlendMode blendMode = BlendMode::Opaque;
            s_q3shaders.getBaseTextureName("textures/" + buildSurface.materialName, mapParam, blendMode);

            Material material;
            material.m_name = buildSurface.materialName;
            material.blendMode = blendMode;
            material.albedoMap = getTexture(buildSurface.materialName).handle;

            slotIt = materialSlotByTexture.emplace(
                buildSurface.materialName, static_cast<uint32_t>(m_materials.size())).first;
            m_materials.push_back(std::move(material));
        }

        Surface surface;
        surface.m_name = "surface_" + buildSurface.materialName;
        surface.materialSlot = slotIt->second;
        surface.startIndex = buildSurface.startIndex;
        surface.count = buildSurface.count;
        m_surfaces.push_back(std::move(surface));
        m_surfaceBounds.push_back(buildSurface.bounds);
    }

    m_lightmapAtlasSize = build.lightmapAtlasSize;
    m_positions = std::move(build.positions);
    m_indices = std::move(build.indices);
    m_lightmapTexCoords = std::move(build.lightmapTexCoords);
    m_normals.reserve(build.shading.size());
    for (const auto& shading : build.shading)
    {
        m_normals.push_back(shading.normal);
    }

    m_vao = std::make_unique<VertexArray>();

    m_posBuffer = std::make_unique<VertexBuffer>(
        m_positions.data(), m_positions.size() * sizeof(VertexPosition));
    m_posBuffer->setLayout({
        {VertexAttributeLocation::Position, ShaderDataType::Float3}
    });
    m_vao->setVertexBuffer(m_posBuffer.get());

    m_shadingBuffer = std::make_unique<VertexBuffer>(
        build.shading.data(), build.shading.size() * sizeof(VertexShading));
    m_shadingBuffer->setLayout({
        {VertexAttributeLocation::Normal, ShaderDataType::Float3},
        {VertexAttributeLocation::TexCoord, ShaderDataType::Float2}
    });
    m_vao->setVertexBuffer(m_shadingBuffer.get());

    m_lightmapBuffer = std::make_unique<VertexBuffer>(
        m_lightmapTexCoords.data(), m_lightmapTexCoords.size() * sizeof(glm::vec2));
    m_lightmapBuffer->setLayout({
        {VertexAttributeLocation::TexCoord2, ShaderDataType::Float2}
    });
    m_vao->setVertexBuffer(m_lightmapBuffer.get());

    m_indexBuffer = std::make_unique<IndexBuffer>(m_indices.data(), m_indices.size());
    m_vao->setIndexBuffer(m_indexBuffer.get());

    // белая текстура
    uploadLightmapTexture(nullptr);
    
    std::cout << "[MapMesh] Created GPU buffers. Vertices: " << m_positions.size() << ", surfaces: " << m_surfaces.size() << std::endl;
    return true;
}

void MapMesh::uploadLightmapTexture(const uint16_t* halfRgb)
{
    const GLuint previousTexture = m_lightmapTexture;
    
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    if (halfRgb)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_lightmapAtlasSize, m_lightmapAtlasSize, 0, GL_RGB, GL_HALF_FLOAT, halfRgb);
    }
    else
    {
        uint16_t white[3] = { 0x3C00, 0x3C00, 0x3C00 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 1, 1, 0, GL_RGB, GL_HALF_FLOAT, white);
    }
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    m_lightmapTexture = texture;
    if (previousTexture != 0)
    {
        glDeleteTextures(1, &previousTexture);
    }
    
    GLState::invalidate();
}

bool MapMesh::updateLightmapTexture(const std::vector<uint16_t>& halfRgb, std::string& error)
{
    if (m_lightmapAtlasSize <= 0)
    {
        error = "Map mesh has no lightmap atlas";
        return false;
    }
    
    const size_t expected = static_cast<size_t>(m_lightmapAtlasSize) * m_lightmapAtlasSize * 3;
    
    if (halfRgb.size() != expected)
    {
        error = "Lightmap pixel data does not match the atlas size";
        return false;
    }
    
    uploadLightmapTexture(halfRgb.data());
    m_revision = s_nextRevision++;   // RenderScene перестроит статику с новым хэндлом текстуры
    error.clear();
    
    return true;
}
