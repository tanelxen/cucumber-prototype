//
//  ModelLoader.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.07.26.
//

#include "ModelLoader.h"

#include "Mesh.h"
#include "Animator.h"
#include "Skeleton.h"

#define CGLTF_IMPLEMENTATION
#include "../vendor/cgltf.h"

#include "../vendor/stb_image.h"

#include <glm/glm.hpp>

#include <iostream>

static void loadGLTFWithSurfaces(cgltf_data* data,
                                 std::vector<glm::vec3>& outPositions,
                                 std::vector<glm::vec3>& outNormals,
                                 std::vector<glm::vec2>& outTexCoords,
                                 std::vector<glm::u8vec4>& outBoneIndices,
                                 std::vector<glm::u8vec4>& outWeights,
                                 std::vector<uint32_t>& outIndices,
                                 std::vector<Surface>& outSurfaces);


static std::vector<Material> loadGLTFMaterials(cgltf_data* data, const std::string& gltfFilePath);
static std::vector<AnimationClip> loadGLTFAnimations(cgltf_data* data, const std::unordered_map<cgltf_node*, int>& nodeToBoneIndex);

namespace ModelLoader
{
    bool LoadGLTF(const std::string& path, Mesh& model, Animator& animator)
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
        
        std::vector<glm::u8vec4> boneIndices;
        std::vector<glm::u8vec4> weights;
        
        loadGLTFWithSurfaces(data, positions, normals, texCoords, boneIndices, weights, indices, surfaces);
        
        auto size = positions.size();
        std::vector<VertexPosition> vertGeometry(size);
        std::vector<VertexShading> vertShading(size);
        std::vector<VertexSkinning> vertSkinning(size);
        
        for (int i = 0; i < size; ++i)
        {
            vertGeometry[i] = { .position = positions[i] };
            vertShading[i] = { .normal = normals[i], .texCoord = texCoords[i] };
            
            if (i < boneIndices.size())
            {
                vertSkinning[i] = { .boneIndices = boneIndices[i], .boneWeights = weights[i] };
            }
        }
        
        model.setVertexData(vertGeometry, vertShading, &vertSkinning);
        model.setIndices(indices);
        model.setSurfaces(surfaces);
        
        std::unordered_map<cgltf_node*, int> nodeToBoneIndex;
        
        // Загрузка скелета
        if (data->skins_count > 0)
        {
            cgltf_skin& skin = data->skins[0];
            
            auto skeleton = std::make_shared<Skeleton>();
            skeleton->bones.resize(skin.joints_count);
            
            // Заполняем nodeToBoneIndex
            for (cgltf_size i = 0; i < skin.joints_count; ++i) {
                nodeToBoneIndex[skin.joints[i]] = static_cast<int>(i);
            }
            
            for (cgltf_size i = 0; i < skin.joints_count; ++i) {
                cgltf_node* jointNode = skin.joints[i];
                if (jointNode->parent && nodeToBoneIndex.count(jointNode->parent)) {
                    skeleton->bones[i].parentIndex = nodeToBoneIndex[jointNode->parent];
                }
            }
            
            if (skin.inverse_bind_matrices) {
                cgltf_accessor* ibmAcc = skin.inverse_bind_matrices;
                for (cgltf_size i = 0; i < skin.joints_count; ++i) {
                    cgltf_accessor_read_float(ibmAcc, i, (float*)&skeleton->bones[i].inverseBindMatrix, 16);
                }
            }
            
            animator.setSkeleton(skeleton);
        }
        
        // Загрузка анимаций (передаем data и сформированную мапу узлов)
        if (data->animations_count > 0)
        {
            auto animations = loadGLTFAnimations(data, nodeToBoneIndex);
            
            for (auto clip : animations)
            {
                animator.addClip(clip);
            }
        }
        
        cgltf_free(data);
        return true;
    }
}

static void loadGLTFWithSurfaces(cgltf_data* data,
                                 std::vector<glm::vec3>& outPositions,
                                 std::vector<glm::vec3>& outNormals,
                                 std::vector<glm::vec2>& outTexCoords,
                                 std::vector<glm::u8vec4>& outBoneIndices,
                                 std::vector<glm::u8vec4>& outWeights,
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
            
            if (primitive.type != cgltf_primitive_type_triangles) continue;
            
            Surface surface;
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
            
            cgltf_accessor* jointsAccessor = nullptr;
            cgltf_accessor* weightsAccessor = nullptr;
            
            for (cgltf_size k = 0; k < primitive.attributes_count; ++k)
            {
                cgltf_attribute& attr = primitive.attributes[k];
                if (attr.type == cgltf_attribute_type_position)  posAccessor = attr.data;
                if (attr.type == cgltf_attribute_type_normal)    normAccessor = attr.data;
                if (attr.type == cgltf_attribute_type_texcoord)  uvAccessor = attr.data;
                
                if (attr.type == cgltf_attribute_type_joints)  jointsAccessor = attr.data;
                if (attr.type == cgltf_attribute_type_weights) weightsAccessor = attr.data;
            }
            
            if (!posAccessor) continue;
            
            if (posAccessor)
            {
                uint8_t* bufferPtr = (uint8_t*)posAccessor->buffer_view->buffer->data
                + posAccessor->buffer_view->offset
                + posAccessor->offset;
                
                glm::vec3* sourcePositions = (glm::vec3*)bufferPtr;
                
                outPositions.insert(outPositions.end(), sourcePositions, sourcePositions + posAccessor->count);
            }
            
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
            
            if (jointsAccessor)
            {
                if (jointsAccessor->component_type == cgltf_component_type_r_8u)
                {
                    uint8_t* bufferPtr = (uint8_t*)jointsAccessor->buffer_view->buffer->data
                    + jointsAccessor->buffer_view->offset
                    + jointsAccessor->offset;
                    
                    glm::u8vec4* boneIndices = (glm::u8vec4*)bufferPtr;
                    
                    outBoneIndices.insert(outBoneIndices.end(), boneIndices, boneIndices + jointsAccessor->count);
                }
            }
            
            if (weightsAccessor)
            {
                size_t count = weightsAccessor->count;
                std::vector<glm::u8vec4> weights(count);
                
                for (cgltf_size k = 0; k < count; ++k)
                {
                    float w[4];
                    cgltf_accessor_read_float(weightsAccessor, k, w, 4);
                    weights[k] = glm::u8vec4(w[0] * 255, w[1] * 255, w[2] * 255, w[3] * 255);
                }
                
                outWeights.insert(outWeights.end(), weights.begin(), weights.end());
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

static uint32_t loadSingleGLTFTexture(const cgltf_image& img, const std::string& baseDir)
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

static std::vector<Material> loadGLTFMaterials(cgltf_data* data, const std::string& gltfFilePath)
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

static std::vector<AnimationClip> loadGLTFAnimations(cgltf_data* data, const std::unordered_map<cgltf_node*, int>& nodeToBoneIndex)
{
    std::vector<AnimationClip> animations;
    
    for (cgltf_size i = 0; i < data->animations_count; ++i)
    {
        cgltf_animation& gltfAnim = data->animations[i];
        
        AnimationClip clip;
        clip.name = gltfAnim.name ? gltfAnim.name : "Animation_" + std::to_string(i);
        clip.duration = 0.0f;
        
        // Временная мапа для сбора данных разных каналов (Translation, Rotation, Scale) в один трек для конкретной кости
        std::unordered_map<int, AnimationTrack> trackMap;
        
        for (cgltf_size c = 0; c < gltfAnim.channels_count; ++c)
        {
            cgltf_animation_channel& channel = gltfAnim.channels[c];
            cgltf_node* targetNode = channel.target_node;
            
            // Если узел не является частью скелета (не найден в мапе), пропускаем
            if (!targetNode || nodeToBoneIndex.find(targetNode) == nodeToBoneIndex.end())
                continue;
            
            int boneIndex = nodeToBoneIndex.at(targetNode);
            AnimationTrack& track = trackMap[boneIndex];
            track.boneIndex = boneIndex;
            
            cgltf_animation_sampler* sampler = channel.sampler;
            cgltf_accessor* input = sampler->input;   // Время (таймстемпы)
            cgltf_accessor* output = sampler->output; // Значения ключевых кадров
            
            // 1. Считываем временные метки
            std::vector<float> times(input->count);
            for (cgltf_size k = 0; k < input->count; ++k)
            {
                cgltf_accessor_read_float(input, k, &times[k], 1);
                // Обновляем общую длительность клипа
                if (times[k] > clip.duration) {
                    clip.duration = times[k];
                }
            }
            
            // 2. Считываем значения в зависимости от типа трансформации
            if (channel.target_path == cgltf_animation_path_type_translation)
            {
                track.positionTimes = times;
                track.positions.resize(output->count);
                
                for (cgltf_size k = 0; k < output->count; ++k) {
                    cgltf_accessor_read_float(output, k, (float*)&track.positions[k], 3);
                }
            }
            else if (channel.target_path == cgltf_animation_path_type_rotation)
            {
                track.rotationTimes = times;
                track.rotations.resize(output->count);
                
                for (cgltf_size k = 0; k < output->count; ++k) {
                    float q[4];
                    cgltf_accessor_read_float(output, k, q, 4);
                    
                    // ВАЖНО: glTF хранит кватернионы в формате [X, Y, Z, W]
                    // Конструктор glm::quat по умолчанию принимает [W, X, Y, Z]
                    track.rotations[k] = glm::quat(q[3], q[0], q[1], q[2]);
                }
            }
            else if (channel.target_path == cgltf_animation_path_type_scale)
            {
                track.scaleTimes = times;
                track.scales.resize(output->count);
                
                for (cgltf_size k = 0; k < output->count; ++k) {
                    cgltf_accessor_read_float(output, k, (float*)&track.scales[k], 3);
                }
            }
        }
        
        // Переносим сформированные треки из мапы в итоговый массив клипа
        for (auto& pair : trackMap) {
            clip.tracks.push_back(pair.second);
        }
        
        animations.push_back(clip);
    }
    
    return animations;
}
