//
//  DecalManager.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#include "DecalManager.h"
#include "ShaderVariant.h"

#include "../vendor/stb_image.h"
#include <glad/glad.h>

#include <vector>
#include <algorithm>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

static const char* decal_vs = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

// Наша инстанс матрица мира собранная из 4-х локаций
layout (location = 2) in mat4 iModelMatrix;
layout (location = 6) in float iAlpha;

out vec2 TexCoords;
out float DecalAlpha;

uniform mat4 view;
uniform mat4 projection;

void main() {
    TexCoords = aTexCoords;
    DecalAlpha = iAlpha;
    
    gl_Position = projection * view * iModelMatrix * vec4(aPos, 1.0);
}
)";

static const char* decal_fs = R"(
#version 410 core
in vec2 TexCoords;
in float DecalAlpha;

out vec4 FragColor;
uniform sampler2D decalTexture;

void main() {
    vec4 texColor = texture(decalTexture, TexCoords);
    
    // Мягко растворяем пиксели на основе времени жизни декали
    FragColor = vec4(texColor.rgb, texColor.a * DecalAlpha);
}
)";

namespace DecalManager {
    
    struct Vertex2D {
        glm::vec3 position;
        glm::vec2 texCoords;
    };
    
    struct Decal {
        glm::vec3 position;
        glm::vec3 normal;
        float size;
        float life;
        float maxLife;
        int textureId;
        
        // Посчитанная матрица мира для этого кадра
        glm::mat4 modelMatrix;
    };
    
    struct BatchRange {
        int textureId;
        uint32_t startIndex;
        uint32_t count;
    };
    
    const int MAX_DECALS = 1000; // Лимит дырок от пуль на уровне
    static std::vector<Decal> s_decalPool;
    static std::vector<BatchRange> s_batches;
    
    static GLuint s_quadVAO = 0, s_quadVBO = 0, s_quadEBO = 0, s_instanceVBO = 0;
    static std::unique_ptr<ShaderVariant> s_decalShader = nullptr;
    
    void Init() {
        s_decalPool.reserve(MAX_DECALS);
        
        Vertex2D quadVertices[] = {
            { glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec2(0.0f, 1.0f) },
            { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 0.0f) },
            { glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 0.0f) },
            { glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec2(1.0f, 1.0f) }
        };
        unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };
        
        glGenVertexArrays(1, &s_quadVAO);
        glGenBuffers(1, &s_quadVBO);
        glGenBuffers(1, &s_quadEBO);
        
        glBindVertexArray(s_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_quadEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, texCoords));
        
        // Создаем динамический VBO для инстансинга матриц и альфы
        glGenBuffers(1, &s_instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, s_instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, MAX_DECALS * (sizeof(glm::mat4) + sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
        
        // Передаем mat4 в шейдер через 4 локации (по vec4 на каждую колонку матрицы)
        GLsizei stride = sizeof(glm::mat4) + sizeof(float);
        for (unsigned int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(i * sizeof(glm::vec4)));
            glVertexAttribDivisor(2 + i, 1);
        }
        
        // Локация 6: Альфа-канал для плавного растворения
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::mat4)));
        glVertexAttribDivisor(6, 1);
        
        glBindVertexArray(0);
        
        // Инициализируем шейдер декалей
        s_decalShader = std::make_unique<ShaderVariant>();
        s_decalShader->init(decal_vs, decal_fs);
    }
    
    void AddDecal(int textureId, glm::vec3 position, glm::vec3 normal, float size, float lifespan) {
        // Если пул забит, удаляем самую старую декаль (первую в списке)
        if (s_decalPool.size() >= MAX_DECALS) {
            s_decalPool.erase(s_decalPool.begin());
        }
        
        Decal d;
        d.position = position;
        d.normal = glm::normalize(normal);
        d.size = size;
        d.maxLife = lifespan;
        d.life = lifespan;
        d.textureId = textureId;
        
        // ВЫЧИСЛЕНИЕ МАТРИЦЫ ТРАНСФОРМАЦИИ (Разворот квада вдоль стены)
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        // Защита от коллинеарности: если нормаль стены смотрит строго вверх или вниз
        if (glm::abs(glm::dot(d.normal, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        
        glm::vec3 right = glm::normalize(glm::cross(up, d.normal));
        glm::vec3 actualUp = glm::cross(d.normal, right);
        
        glm::mat4 rotationMatrix(1.0f);
        rotationMatrix[0] = glm::vec4(right, 0.0f);
        rotationMatrix[1] = glm::vec4(actualUp, 0.0f);
        rotationMatrix[2] = glm::vec4(d.normal, 0.0f); // Ось Z квада смотрит из стены
        
        // Сдвигаем декаль чуть-чуть ВПЕРЕД по нормали (на 0.002 метра), чтобы избежать Z-Fighting
        glm::vec3 offsetPos = d.position + d.normal * 0.1f;
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), offsetPos)
        * rotationMatrix
        * glm::scale(glm::mat4(1.0f), glm::vec3(d.size));
        
        d.modelMatrix = model;
        s_decalPool.push_back(d);
    }
    
    void Update(float dt) {
        // Идем с конца буфера, чтобы безопасно удалять мертвые декали
        for (int i = static_cast<int>(s_decalPool.size()) - 1; i >= 0; i--) {
            s_decalPool[i].life -= dt;
            if (s_decalPool[i].life <= 0.0f) {
                s_decalPool.erase(s_decalPool.begin() + i);
            }
        }
    }
    
    void Flush(glm::mat4 view, glm::mat4 projection)
    {
        if (s_decalPool.empty()) return;
        
        // 1. Сортируем декали по текстурам, чтобы минимизировать смену стейта GPU
        std::sort(s_decalPool.begin(), s_decalPool.end(), [](const Decal& a, const Decal& b) {
            return a.textureId < b.textureId;
        });
        
        // 2. Нарезаем батчи
        s_batches.clear();
        int currentTex = s_decalPool[0].textureId;
        uint32_t startIdx = 0;
        
        // Буфер для упаковки данных VBO (Матрица + Альфа)
        struct InstanceData {
            glm::mat4 model;
            float alpha;
        };
        std::vector<InstanceData> vboData;
        vboData.reserve(s_decalPool.size());
        
        for (size_t i = 0; i < s_decalPool.size(); ++i) {
            // Расчет альфы для плавного растворения за последние 2 секунды жизни
            float alpha = 1.0f;
            if (s_decalPool[i].life < 2.0f) {
                alpha = glm::clamp(s_decalPool[i].life / 2.0f, 0.0f, 1.0f);
            }
            vboData.push_back({ s_decalPool[i].modelMatrix, alpha });
            
            if (s_decalPool[i].textureId != currentTex) {
                s_batches.push_back({ currentTex, startIdx, static_cast<uint32_t>(i - startIdx) });
                currentTex = s_decalPool[i].textureId;
                startIdx = static_cast<uint32_t>(i);
            }
        }
        s_batches.push_back({ currentTex, startIdx, static_cast<uint32_t>(s_decalPool.size() - startIdx) });
        
        // 3. Выгружаем все матрицы инстансов в GPU одним махом
        glBindBuffer(GL_ARRAY_BUFFER, s_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vboData.size() * sizeof(InstanceData), vboData.data());
        
        // 4. Отрисовка
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Стандартный альфа-блендинг для дырок от пуль
        
        glDepthMask(GL_FALSE);      // Отключаем запись в буфер глубины
        glEnable(GL_DEPTH_TEST);    // Но продолжаем читать этот буфер
        
//        glEnable(GL_POLYGON_OFFSET_FILL);
//        glPolygonOffset(-8, 1);
        
        s_decalShader->bind();
        s_decalShader->setUniform("view", view);
        s_decalShader->setUniform("projection", projection);
        
        glBindVertexArray(s_quadVAO);
        
        GLsizei stride = sizeof(InstanceData);
        
        for (const auto& batch : s_batches)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, batch.textureId);
            
            // Сдвигаем базовые указатели VBO под конкретный батч
            uintptr_t byteOffset = batch.startIndex * sizeof(InstanceData);
            for (unsigned int i = 0; i < 4; i++) {
                glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset + i * sizeof(glm::vec4)));
            }
            glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset + sizeof(glm::mat4)));
            
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, batch.count);
        }
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
//        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_TRUE);
        
    }
    
    void Shutdown()
    {
        s_decalPool.clear();
        s_batches.clear();
        if (s_quadVAO) glDeleteVertexArrays(1, &s_quadVAO);
        if (s_quadVBO) glDeleteBuffers(1, &s_quadVBO);
        if (s_quadEBO) glDeleteBuffers(1, &s_quadEBO);
        if (s_instanceVBO) glDeleteBuffers(1, &s_instanceVBO);
        s_decalShader.reset();
    }
}

