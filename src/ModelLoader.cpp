//
//  ModelLoader.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.07.26.
//

#include "ModelLoader.h"
#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../vendor/tiny_obj_loader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <glm/glm.hpp>

#include <unordered_map>
#include <iostream>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
    
    bool operator==(const Vertex& other) const {
        return pos == other.pos &&
        normal == other.normal &&
        texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            size_t h1 = hash<glm::vec3>()(vertex.pos);
            size_t h2 = hash<glm::vec3>()(vertex.normal);
            size_t h3 = hash<glm::vec2>()(vertex.texCoord);
            
            // Комбинируем хэши
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
        }
    };
}

namespace ModelLoader {
    
    bool LoadOBJ(const std::string& filepath, Mesh& mesh)
    {
        tinyobj::ObjReaderConfig reader_config;
//        reader_config.mtl_search_path = "./"; // Path to material files
        
        tinyobj::ObjReader reader;
        
        if (!reader.ParseFromFile(filepath, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            return 1;
        }
        
        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }
        
        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        //    auto& materials = reader.GetMaterials();
        
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> out_texCoords;
        std::vector<uint32_t> indices;
        
        // Карта для отслеживания уже добавленных уникальных вершин
        std::unordered_map<Vertex, uint32_t> uniqueVertices;
        
        int index = 0;
        
        for (const auto& shape : shapes)
        {
            size_t index_offset = 0;
            
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
                for (size_t v = 0; v < 3; v++) {
                    tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                    
                    Vertex vertex{};
                    
                    // 1. Позиция
                    vertex.pos = {
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]
                    };
                    
                    // 2. Нормаль
                    if (idx.normal_index >= 0) {
                        vertex.normal = {
                            attrib.normals[3 * idx.normal_index + 0],
                            attrib.normals[3 * idx.normal_index + 1],
                            attrib.normals[3 * idx.normal_index + 2]
                        };
                    } else {
                        vertex.normal = glm::vec3(0.0f);
                    }
                    
                    // 3. Текстурные координаты (UV)
                    if (idx.texcoord_index >= 0) {
                        vertex.texCoord = {
                            attrib.texcoords[2 * idx.texcoord_index + 0],
                            1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] // Инвертируем Y (V) для OpenGL
                        };
                    } else {
                        vertex.texCoord = glm::vec2(0.0f);
                    }
                    
                    // Проверка на дубликаты
                    if (uniqueVertices.count(vertex) == 0) {
                        uniqueVertices[vertex] = static_cast<uint32_t>(positions.size());
                        
                        positions.push_back(vertex.pos);
                        normals.push_back(vertex.normal);
                        out_texCoords.push_back(vertex.texCoord); // Сохраняем UV координату
                    }
                    
                    indices.push_back(uniqueVertices[vertex]);
                }
                index_offset += 3;
            }
        }
        
        mesh.init(positions, normals, indices);
    }
}
