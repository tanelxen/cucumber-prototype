//
//  ModelLoader.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.07.26.
//

#include "ModelLoader.h"
#include "Mesh.h"

#include "RenderDevice.h"

#define CGLTF_IMPLEMENTATION
#include "../vendor/cgltf.h"

#include "../vendor/stb_image.h"

#include <glm/glm.hpp>

#include <iostream>

void loadGLTFWithSurfaces(cgltf_data* data,
                          std::vector<glm::vec3>& outPositions,
                          std::vector<glm::vec3>& outNormals,
                          std::vector<glm::vec2>& outTexCoords,
                          std::vector<uint32_t>& outIndices,
                          std::vector<Surface>& outSurfaces);

std::vector<Material> loadGLTFMaterials(cgltf_data* data, const std::string& gltfFilePath);

namespace ModelLoader
{
    bool LoadGLTF(const std::string& path, Mesh& model)
    {
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        
        if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) return false;
        
        if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success)
        {
            cgltf_free(data);
            return false;
        }
        
        model.m_name = path;
        model.m_materials = loadGLTFMaterials(data, path);
        
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<uint32_t> indices;
        std::vector<Surface> surfaces;
        
        loadGLTFWithSurfaces(data, positions, normals, texCoords, indices, surfaces);
        model.init(positions, normals, texCoords, indices);
        model.setSurfaces(surfaces);
        
        cgltf_free(data);
    }
}

void loadGLTFWithSurfaces(cgltf_data* data,
                          std::vector<glm::vec3>& outPositions,
                          std::vector<glm::vec3>& outNormals,
                          std::vector<glm::vec2>& outTexCoords,
                          std::vector<uint32_t>& outIndices,
                          std::vector<Surface>& outSurfaces)
{
    uint32_t vertexOffset = 0;
    
    for (cgltf_size i = 0; i < data->meshes_count; ++i)
    {
        cgltf_mesh& mesh = data->meshes[i];
        
        std::string meshName = mesh.name ? mesh.name : "Mesh_" + std::to_string(i);
        
        for (cgltf_size j = 0; j < mesh.primitives_count; ++j)
        {
            cgltf_primitive& primitive = mesh.primitives[j];
            
            Surface surface{};
            surface.startIndex = static_cast<uint32_t>(outIndices.size());
            surface.m_name = meshName + "_prim_" + std::to_string(j);
            
            if (primitive.material != nullptr)
            {
                ptrdiff_t matIndex = primitive.material - data->materials;
                surface.materialSlot = static_cast<uint32_t>(matIndex);
            }
            else
            {
                surface.materialSlot = 0;
            }
            
            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;
            cgltf_accessor* uvAccessor = nullptr;
            
            for (cgltf_size k = 0; k < primitive.attributes_count; ++k)
            {
                cgltf_attribute& attr = primitive.attributes[k];
                if (attr.type == cgltf_attribute_type_position)  posAccessor = attr.data;
                if (attr.type == cgltf_attribute_type_normal)    normAccessor = attr.data;
                if (attr.type == cgltf_attribute_type_texcoord)  uvAccessor = attr.data;
            }
            
            if (!posAccessor) continue;
            
            if (posAccessor)
            {
                // Получаем прямой указатель на начало данных в памяти
                uint8_t* bufferPtr = (uint8_t*)posAccessor->buffer_view->buffer->data
                + posAccessor->buffer_view->offset
                + posAccessor->offset;
                
                // Приводим его к типу ваших вершин (glm::vec3)
                glm::vec3* sourcePositions = (glm::vec3*)bufferPtr;
                
                // Вставляем весь массив целиком за один шаг
                outPositions.insert(outPositions.end(), sourcePositions, sourcePositions + posAccessor->count);
            }
            
            // 2. Быстрое копирование НОРМАЛЕЙ
            if (normAccessor)
            {
                uint8_t* bufferPtr = (uint8_t*)normAccessor->buffer_view->buffer->data
                + normAccessor->buffer_view->offset
                + normAccessor->offset;
                
                glm::vec3* sourceNormals = (glm::vec3*)bufferPtr;
                outNormals.insert(outNormals.end(), sourceNormals, sourceNormals + normAccessor->count);
            }
            else
            {
                outNormals.insert(outNormals.end(), posAccessor->count, glm::vec3(0.0f, 1.0f, 0.0f));
            }
            
            // 3. Быстрое копирование ТЕКСТУРНЫХ КООРДИНАТ
            if (uvAccessor)
            {
                uint8_t* bufferPtr = (uint8_t*)uvAccessor->buffer_view->buffer->data
                + uvAccessor->buffer_view->offset
                + uvAccessor->offset;
                
                glm::vec2* sourceUVs = (glm::vec2*)bufferPtr;
                outTexCoords.insert(outTexCoords.end(), sourceUVs, sourceUVs + uvAccessor->count);
            }
            else
            {
                outTexCoords.insert(outTexCoords.end(), posAccessor->count, glm::vec2(0.0f));
            }
            
            size_t vertexCount = posAccessor->count;
            
            if (primitive.indices)
            {
                cgltf_accessor* indexAccessor = primitive.indices;
                surface.count = static_cast<uint32_t>(indexAccessor->count);
                
                for (cgltf_size idx = 0; idx < indexAccessor->count; ++idx)
                {
                    uint32_t index = static_cast<uint32_t>(cgltf_accessor_read_index(indexAccessor, idx));
                    outIndices.push_back(index + vertexOffset);
                }
            }
            else
            {
                surface.count = static_cast<uint32_t>(vertexCount);
                
                for (uint32_t idx = 0; idx < static_cast<uint32_t>(vertexCount); ++idx)
                {
                    outIndices.push_back(idx + vertexOffset);
                }
            }
            
            outSurfaces.push_back(surface);
            vertexOffset += static_cast<uint32_t>(vertexCount);
        }
    }
}

uint32_t loadSingleGLTFTexture(const cgltf_image& img, const std::string& baseDir)
{
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixelData = nullptr;
    
    if (img.uri != nullptr)
    {
        std::string fullPath = baseDir + "/" + img.uri;
        
        // glTF использует URL-кодирование. Заменяем "%20" обратно на пробелы для файловой системы
        size_t pos;
        while ((pos = fullPath.find("%20")) != std::string::npos) {
            fullPath.replace(pos, 3, " ");
        }
        
        // Загружаем всегда 4 канала (RGBA) для унификации, либо 0 для автоматического определения
        pixelData = stbi_load(fullPath.c_str(), &width, &height, &channels, 4);
        if (!pixelData) {
            std::cerr << "[stb_image] Не удалось загрузить внешний файл: " << fullPath << std::endl;
            return 0;
        }
        
        channels = 4;
    }
    else if (img.buffer_view != nullptr)
    {
        cgltf_buffer_view* view = img.buffer_view;
        
        const uint8_t* rawData = static_cast<const uint8_t*>(view->buffer->data) + view->offset;
        size_t dataSize = view->size;
        
        pixelData = stbi_load_from_memory(rawData, static_cast<int>(dataSize), &width, &height, &channels, 4);
        if (!pixelData) {
            std::cerr << "[stb_image] Не удалось декодировать встроенную текстуру из GLB" << std::endl;
            return 0;
        }
        channels = 4;
    }
    
    if (!pixelData) return 0;
    
    bool isSRGB = true;
    bool hasAlpha = (channels == 4);
    
    uint32_t textureHandle = RenderDevice::makeTexture(width, height, hasAlpha, isSRGB, pixelData);
    
    stbi_image_free(pixelData);
    
    return textureHandle;
}

std::vector<Material> loadGLTFMaterials(cgltf_data* data, const std::string& gltfFilePath)
{
    std::vector<Material> outMaterials;
    
    std::vector<uint32_t> textures(data->images_count, 0);
    std::string baseDir = gltfFilePath.substr(0, gltfFilePath.find_last_of("/\\"));
    
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        textures[i] = loadSingleGLTFTexture(data->images[i], baseDir);
    }
    
    if (data->materials_count == 0)
    {
        outMaterials.push_back(Material());
        return outMaterials;
    }
    
    for (cgltf_size i = 0; i < data->materials_count; ++i)
    {
        cgltf_material& gltfMat = data->materials[i];
        
        Material material;
        material.m_name = gltfMat.name;
        
        if (gltfMat.has_pbr_metallic_roughness)
        {
            cgltf_pbr_metallic_roughness& pbr = gltfMat.pbr_metallic_roughness;
            
            material.tintColor.x = pbr.base_color_factor[0];
            material.tintColor.y = pbr.base_color_factor[1];
            material.tintColor.z = pbr.base_color_factor[2];
            material.tintColor.w = pbr.base_color_factor[3];
            
            if (pbr.base_color_texture.texture != nullptr)
            {
                cgltf_texture* tex = pbr.base_color_texture.texture;
                
                if (tex->image != nullptr)
                {
                    ptrdiff_t index = tex->image - data->images;
                    material.albedoMap = textures[index];
                }
            }
        }
        
        outMaterials.push_back(material);
    }
    
    return outMaterials;
}
