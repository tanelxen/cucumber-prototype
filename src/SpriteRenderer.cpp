//
//  SpriteRenderer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#include "SpriteRenderer.h"
#include "ShaderVariant.h"

#include "../vendor/stb_image.h"
#include <glad/glad.h>

#include <unordered_map>
#include <iostream>

// Структура вершины для VBO
struct SpriteVertex {
    glm::vec3 position;  // Локальные координаты квада (-0.5 до 0.5)
    glm::vec2 texCoords; // UV
};

// Данные экземпляра биллборда
struct BillboardInstance {
    glm::vec3 worldPos;
    float scale;
    float zRot;
};

static const char* billboard_vs = R"(
#version 410 core

layout (location = 0) in vec3 aPos;        // Локальный квад (-0.5 до 0.5)
layout (location = 1) in vec2 aTexCoords;  // UV

// Данные инстанса из s_instanceVBO:
layout (location = 2) in vec3 iWorldPos;   // Мировая позиция дула оружия
layout (location = 3) in float iScale;     // Масштаб вспышки
layout (location = 4) in float iZRot;      // Случайный поворот вспышки

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;

    // Переводим мировую позицию вспышки в пространство камеры
    vec4 viewPosition = view * vec4(iWorldPos, 1.0);

    // Матрица вращения вокруг оси Z прямо на экране
    float c = cos(iZRot);
    float s = sin(iZRot);
    mat2 rotMatrix = mat2(c, -s, s, c);
    
    // Поворачиваем локальную вершину квада и умножаем на кастомный масштаб
    vec2 rotatedPos = rotMatrix * aPos.xy * iScale;

    // Прибавляем смещение к позиции в пространстве камеры (классический биллборд)
    viewPosition.xy += rotatedPos;

    gl_Position = projection * viewPosition;
}
)";

static const char* billboard_fs = R"(
    #version 410 core
    in vec2 TexCoords;
    out vec4 FragColor;
    uniform sampler2D uTexture;
    void main() {
        FragColor = texture(uTexture, TexCoords);;
    }
)";

namespace SpriteRenderer {
    
    // Структура геометрии одного квада
    struct Vertex2D {
        glm::vec3 position;
        glm::vec2 texCoords;
    };
    
    // Данные, уникальные для каждого биллборда на экране
    struct BillboardInstance {
        glm::vec3 worldPos;
        float scale;
        float zRot;
    };
    
    struct Batch {
        int textureId = 0;
        std::vector<BillboardInstance> instances;
    };
    
    // Внутреннее состояние модуля (скрыто в cpp)
    static std::unordered_map<int, Batch> s_batches;
    static std::unordered_map<std::string, int> s_loadedTextures;
    
    static GLuint s_quadVAO = 0;
    static GLuint s_quadVBO = 0;
    static GLuint s_quadEBO = 0;
    static GLuint s_instanceVBO = 0;
    
    static std::unique_ptr<ShaderVariant> s_shader = nullptr;
    
    void Init()
    {
        // 1. Геометрия единичного квада (от -0.5 до 0.5, чтобы центр был в (0,0))
        Vertex2D quadVertices[] = {
            { glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec2(0.0f, 1.0f) }, // Топ-лево
            { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 0.0f) }, // Бот-лево
            { glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 0.0f) }, // Бот-право
            { glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec2(1.0f, 1.0f) }  // Топ-право
        };
        
        unsigned int indices[] = {
            0, 1, 2,
            0, 2, 3
        };
        
        // 2. Создаем и настраиваем VAO / VBO / EBO для геометрии квада
        glGenVertexArrays(1, &s_quadVAO);
        glGenBuffers(1, &s_quadVBO);
        glGenBuffers(1, &s_quadEBO);
        
        glBindVertexArray(s_quadVAO);
        
        // Загружаем вершины квада
        glBindBuffer(GL_ARRAY_BUFFER, s_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        // Загружаем индексы
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_quadEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        
        // Атрибут 0: Позиция вершины квада (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, position));
        
        // Атрибут 1: Текстурные координаты UV (vec2)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, texCoords));
        
        // 3. Создаем DYNAMIC VBO для данных инстансов (сдвиг, масштаб, поворот)
        glGenBuffers(1, &s_instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, s_instanceVBO);
        // Выделяем память с запасом (например, под 1000 биллбордов одновременно), данные будем перезаписывать
        glBufferData(GL_ARRAY_BUFFER, 1000 * sizeof(BillboardInstance), nullptr, GL_DYNAMIC_DRAW);
        
        // Атрибут 2: Мировая позиция инстанса (vec3)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), (void*)offsetof(BillboardInstance, worldPos));
        glVertexAttribDivisor(2, 1); // ВАЖНО: 1 означает обновление атрибута для каждого ИНСТАНСА [3]
        
        // Атрибут 3: Масштаб (float)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), (void*)offsetof(BillboardInstance, scale));
        glVertexAttribDivisor(3, 1);
        
        // Атрибут 4: Поворот по Z (float)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), (void*)offsetof(BillboardInstance, zRot));
        glVertexAttribDivisor(4, 1);
        
        glBindVertexArray(0);
        
        // 4. Инициализируем шейдер биллбордов
        s_shader = std::make_unique<ShaderVariant>();
        s_shader->init(billboard_vs, billboard_fs);
    }
    
    int PreloadTexture(const std::string& filename)
    {
        // Если текстура уже загружена — возвращаем кэшированный ID
        if (s_loadedTextures.count(filename)) {
            return s_loadedTextures[filename];
        }
        
        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
        
        if (!data) {
            std::cerr << "[SpriteRenderer] Не удалось загрузить текстуру: " << filename << std::endl;
            return 0;
        }
        
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        stbi_image_free(data);
        
        s_loadedTextures[filename] = textureId;
        return textureId;
    }
    
    void AddBillboard(int textureId, glm::vec3 position, float scale, float zRot)
    {
        if (textureId <= 0) return;
        s_batches[textureId].textureId = textureId;
        s_batches[textureId].instances.push_back({ position, scale, zRot });
    }
    
    void Flush(glm::mat4 view, glm::mat4 projection)
    {
        if (s_batches.empty()) return;
        
        // Настройка стейта под полупрозрачные эффекты / огонь
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Аддитивное смешивание (черный цвет прозрачный) [3]
        glDepthMask(GL_FALSE);             // Вспышки не перекрывают друг друга по буферу глубины [3]
        
        s_shader->bind();
        s_shader->setUniform("view", view);
        s_shader->setUniform("projection", projection);
        
        glBindVertexArray(s_quadVAO);
        
        for (auto& [texId, batch] : s_batches)
        {
            if (batch.instances.empty()) continue;
            
            // Биндим текстуру этого батча
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texId);
            
            // Загружаем массив данных инстансов в s_instanceVBO [3]
            glBindBuffer(GL_ARRAY_BUFFER, s_instanceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, batch.instances.size() * sizeof(BillboardInstance), batch.instances.data());
            
            // Отрисовываем весь батч за 1 Draw Call [3]
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(batch.instances.size()));
            
            batch.instances.clear();
        }
        
        // Возвращаем OpenGL стейт к стандарту движка
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(0);
        
        s_batches.clear();
    }
    
    void Shutdown()
    {
        for (auto& [name, id] : s_loadedTextures) {
            glDeleteTextures(1, (GLuint*)&id);
        }
        s_loadedTextures.clear();
        s_batches.clear();
        
        if (s_quadVAO) glDeleteVertexArrays(1, &s_quadVAO);
        if (s_quadVBO) glDeleteBuffers(1, &s_quadVBO);
        if (s_quadEBO) glDeleteBuffers(1, &s_quadEBO);
        if (s_instanceVBO) glDeleteBuffers(1, &s_instanceVBO);
        
        s_shader.reset();
    }
}

