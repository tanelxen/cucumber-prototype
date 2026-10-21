//
//  MapMesh.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "MapMesh.h"
#include "MapAsset.h"

#include "Camera.h"
#include "Shader.h"

#include "TextureManager.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

#include "ProjectManager.h"

#include <glad/glad.h>
#include "../vendor/stb_image.h"

#include <algorithm>
#include <iostream>

static inline glm::vec3 Quake2OpenGL(const glm::vec3& in)
{
    return { in.x, in.z, -in.y };
}

// Пересечение трех плоскостей для генерации вершин браша
static bool GetMapPlaneIntersection(const MapPlane& p1, const MapPlane& p2, const MapPlane& p3, glm::vec3& outPoint)
{
    glm::vec3 cross12 = glm::cross(p1.normal, p2.normal);
    float denom = glm::dot(cross12, p3.normal);
    
    if (std::abs(denom) < 1e-4f) return false;
    
    outPoint = (glm::cross(p2.normal, p3.normal) * p1.distance +
                glm::cross(p3.normal, p1.normal) * p2.distance +
                cross12 * p3.distance) / denom;
    return true;
}

// Проверка: находится ли сгенерированная вершина внутри выпуклого объема браша
static bool IsPointInsideMapBrush(const glm::vec3& point, const MapBrush& brush)
{
    for (const auto& side : brush.sides)
    {
        // В стандартных .map файлах нормали смотрят СНАРУЖИ браша.
        // Точка внутри, если проекция на нормаль меньше или равна дистанции плоскости (+ погрешность)
        if (glm::dot(side.plane.normal, point) - side.plane.distance > 0.005f) {
            return false;
        }
    }
    return true;
}

static glm::vec2 CalculateValve220UV(const glm::vec3& pos, const MapTextureInfo& tex, int texWidth, int texHeight)
{
    // защита от нулевого масштаба (TrenchBroom трактует 0 как 1)
    const float sx = tex.scaleX != 0.0f ? tex.scaleX : 1.0f;
    const float sy = tex.scaleY != 0.0f ? tex.scaleY : 1.0f;
    
    const float u = glm::dot(pos, tex.uAxis) / sx + tex.uShift;
    const float v = glm::dot(pos, tex.vAxis) / sy + tex.vShift;
    
    return { u / float(texWidth), v / float(texHeight) };
}

MapMesh::MapMesh() = default;
MapMesh::~MapMesh() = default;

void MapMesh::buildFromMapAsset(const MapAsset& mapData)
{
    if (mapData.entities.empty()) return;
    
    struct TemporaryMeshData {
        std::vector<VertexPosition> positions;
        std::vector<VertexShading> shading;
        std::vector<uint32_t> indices;
    };
    
    std::unordered_map<std::string, TemporaryMeshData> groupMap;
    const auto& worldspawn = mapData.entities[0]; // worldspawn
    
    for (const auto& brush : worldspawn.brushes)
    {
        if (brush.sides.size() < 4) continue;
        
        for (size_t i = 0; i < brush.sides.size(); ++i)
        {
            const auto& faceSide = brush.sides[i];
            std::vector<glm::vec3> facePoints;
            
            // 1. Пересечение плоскостей для генерации точек выпуклого многоугольника
            for (size_t j = 0; j < brush.sides.size(); ++j) {
                if (i == j) continue;
                for (size_t k = j + 1; k < brush.sides.size(); ++k) {
                    if (i == k) continue;
                    
                    glm::vec3 pt;
                    if (GetMapPlaneIntersection(faceSide.plane, brush.sides[j].plane, brush.sides[k].plane, pt)) {
                        if (IsPointInsideMapBrush(pt, brush)) {
                            if (std::find_if(facePoints.begin(), facePoints.end(), [&](const glm::vec3& v) {
                                return glm::distance(v, pt) < 0.005f;
                            }) == facePoints.end()) {
                                facePoints.push_back(pt);
                            }
                        }
                    }
                }
            }
            
            if (facePoints.size() < 3) continue;
            

            // 2. Сортировка вершин многоугольника грани по часовой стрелке вокруг нормали
            glm::vec3 center(0.0f);
            for (const auto& p : facePoints) center += p;
            center /= static_cast<float>(facePoints.size());
            
            glm::vec3 n = faceSide.plane.normal;
            
            // Создаем локальный 2D-базис (касательные векторы) на плоскости грани
            glm::vec3 uAxis = glm::normalize(faceSide.texture.uAxis);
            // Если uAxis параллелен нормали или обнулен, генерируем искусственный перпендикуляр
            if (std::abs(glm::dot(uAxis, n)) > 0.9f || glm::length(uAxis) < 0.001f) {
                uAxis = (std::abs(n.z) < 0.9f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            }
            glm::vec3 vAxis = glm::normalize(glm::cross(n, uAxis));
            uAxis = glm::cross(vAxis, n);
            
            // Сортируем с использованием функции atan2 для вычисления угла
            std::sort(facePoints.begin(), facePoints.end(), [&](const glm::vec3& a, const glm::vec3& b) {
                glm::vec3 ca = a - center;
                glm::vec3 cb = b - center;
                
                // Проецируем 3D смещения на наши локальные 2D оси U и V
                float angleA = std::atan2(glm::dot(ca, vAxis), glm::dot(ca, uAxis));
                float angleB = std::atan2(glm::dot(cb, vAxis), glm::dot(cb, uAxis));
                
                // Для обхода по часовой стрелке используем '>', для против — '<'
                return angleA > angleB;
            });
            
            
            std::string texturePath = faceSide.texture.textureName;
            if (texturePath == "__TB_empty" || texturePath == "skip" || texturePath == "clip") {
                continue;
            }
            
            int texWidth = 256;
            int texHeight = 256;
            
            auto textureAsset = TextureManager::Get().loadTexture("wolf/textures/" + texturePath);
            if (textureAsset) {
                texWidth = textureAsset->getWidth();
                texHeight = textureAsset->getHeight();
            }
            
            auto& meshData = groupMap[texturePath];
            uint32_t startIndexOffset = static_cast<uint32_t>(meshData.positions.size());
            
            // 3. Заполнение векторов VertexPosition и VertexShading
            for (const auto& q3Pos : facePoints)
            {
                VertexPosition pos;
                pos.position = Quake2OpenGL(q3Pos);
                
                VertexShading shading;
                shading.normal = Quake2OpenGL(faceSide.plane.normal);
                shading.texCoord = CalculateValve220UV(q3Pos, faceSide.texture, texWidth, texHeight);
                
                meshData.positions.push_back(pos);
                meshData.shading.push_back(shading);
            }
            
            // 4. Триангуляция (Triangle Fan)
            for (size_t t = 1; t < facePoints.size() - 1; ++t)
            {
                meshData.indices.push_back(startIndexOffset);
                meshData.indices.push_back(startIndexOffset + t);
                meshData.indices.push_back(startIndexOffset + t + 1);
            }
        }
    }
    
    // Линеаризация собранных данных для отправки на GPU
    std::vector<VertexPosition> finalPositions;
    std::vector<VertexShading> finalShading;
    std::vector<uint32_t> finalIndices;
    
    uint32_t currentMaterialSlot = 0;
    uint32_t currentIndexOffset = 0;
    
    m_surfaces.clear();
    m_materials.clear();
    
    for (auto& [texName, data] : groupMap)
    {
        if (data.indices.empty()) continue;
        
        // Настройка материала
        Material mat;
        mat.m_name = texName;
        mat.blendMode = BlendMode::Opaque;
        
        auto textureAsset = TextureManager::Get().loadTexture("wolf/textures/" + texName);
        if (textureAsset) {
            mat.albedoMap = textureAsset->getHandle();
        } else {
            mat.albedoMap = 0;
        }
        m_materials.push_back(mat);
        
        // Настройка саб-меша
        Surface surface;
        surface.m_name = "surface_" + texName;
        surface.materialSlot = currentMaterialSlot;
        surface.startIndex = currentIndexOffset;
        surface.count = static_cast<uint32_t>(data.indices.size());
        m_surfaces.push_back(surface);
        
        uint32_t vertexOffset = static_cast<uint32_t>(finalPositions.size());
        finalPositions.insert(finalPositions.end(), data.positions.begin(), data.positions.end());
        finalShading.insert(finalShading.end(), data.shading.begin(), data.shading.end());
        
        for (auto idx : data.indices) {
            finalIndices.push_back(vertexOffset + idx);
        }
        
        currentIndexOffset += surface.count;
        currentMaterialSlot++;
    }
    
    // 5. Ручная инициализация буферов на GPU (в точности как в setVertexData / setIndices)
    m_vao = std::make_unique<VertexArray>();
    
    // Stream 0: Position
    m_posBuffer = std::make_unique<VertexBuffer>(finalPositions.data(), finalPositions.size() * sizeof(VertexPosition));
    m_posBuffer->setLayout({
        { VertexAttributeLocation::Position, ShaderDataType::Float3 }
    });
    m_vao->setVertexBuffer(m_posBuffer.get());
    
    // Stream 1: Shading
    m_shadingBuffer = std::make_unique<VertexBuffer>(finalShading.data(), finalShading.size() * sizeof(VertexShading));
    m_shadingBuffer->setLayout({
        { VertexAttributeLocation::Normal, ShaderDataType::Float3 },
        { VertexAttributeLocation::TexCoord, ShaderDataType::Float2 }
    });
    m_vao->setVertexBuffer(m_shadingBuffer.get());
    
    // Index Buffer
    m_indexBuffer = std::make_unique<IndexBuffer>(finalIndices.data(), finalIndices.size());
    m_vao->setIndexBuffer(m_indexBuffer.get());
    
    std::cout << "[MapMesh] Autonomously created GPU Buffers. Vertices: " << finalPositions.size() << std::endl;
}

void MapMesh::draw(RenderView& view) const
{
    if (!m_vao) return;
    
    for (const auto& surface : m_surfaces)
    {
        const auto& material = m_materials[surface.materialSlot];
        uint32_t features = 0;
        
        if (material.albedoMap > 0)
        {
            features |= ShaderFeature::HasAlbedoMap;
        }
        
        RenderCommand cmd = {
            .vao = m_vao->getID(),
            .start = surface.startIndex,
            .count = surface.count,
            
            .pipeline = PipelineType::Generic,
            .featureMask = features,
            .baseTexture = material.albedoMap,
            .blendMode = material.blendMode,
            .reverseWinding = true
        };
        
        view.submit(cmd);
    }
}
