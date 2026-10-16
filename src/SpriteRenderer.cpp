//
//  SpriteRenderer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#include "SpriteRenderer.h"
#include "ShaderVariant.h"
#include "ProjectManager.h"

#include "../vendor/stb_image.h"
#include <glad/glad.h>

#include <unordered_map>
#include <iostream>

static const char* billboard_vs = R"(
#version 410 core

layout (location = 0) in vec3 aPos;         // Локальный квад (-0.5 до 0.5)
layout (location = 1) in vec2 aTexCoords;   // UV

// Данные инстанса из s_instanceVBO:
layout (location = 2) in vec3 iWorldPos;    // Мировая позиция дула оружия
layout (location = 3) in float iScaleX;     // Для трасера это ширина
layout (location = 4) in float iScaleY;     // Для трасера это длина
layout (location = 5) in vec3 iDirection;
layout (location = 6) in float iType;
layout (location = 7) in vec4 iColor;       // Принимаем цвет инстанса

out vec2 TexCoords;
out vec4 ParticleColor;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;

    if (iType > 0.5) { // ЛОГИКА ТРАСЕРА (Осевой биллборд)
        vec3 p = iWorldPos + iDirection * (aPos.y * iScaleY); // Вытягиваем вдоль направления
        vec3 camPos = vec3(inverse(view)[3]);
        vec3 toCam = normalize(camPos - p);
        vec3 right = normalize(cross(iDirection, toCam)); // Перпендикуляр к камере и пуле
        
        p += right * (aPos.x * iScaleX); // Расширяем в стороны
        gl_Position = projection * view * vec4(p, 1.0);
    } 
    else { // ЛОГИКА ОБЫЧНОГО БИЛЛБОРДА
        vec4 viewPosition = view * vec4(iWorldPos, 1.0);
        float c = cos(iScaleY); // Используем iScaleY как поворот, если обычный
        float s = sin(iScaleY);
        mat2 rotMatrix = mat2(c, -s, s, c);
        vec2 rotatedPos = rotMatrix * aPos.xy * iScaleX;
        viewPosition.xy += rotatedPos;
        gl_Position = projection * viewPosition;
    }

    ParticleColor = iColor;
}
)";

static const char* billboard_fs = R"(
#version 410 core

in vec2 TexCoords;
in vec4 ParticleColor;

out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, TexCoords) * ParticleColor;
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
        
        glm::vec3 direction;
        float type;
        
        glm::vec4 color;
    };
    
    struct Batch {
        int textureId = 0;
        BlendMode blendMode = BlendMode::Alpha;
        std::vector<BillboardInstance> instances;
    };
    
    // Внутреннее состояние модуля (скрыто в cpp)
//    static std::unordered_map<int, Batch> s_batches;
    static std::vector<Batch> s_batches;
    
    static std::unordered_map<std::string, int> s_loadedTextures;
    
    static GLuint s_quadVAO = 0;
    static GLuint s_quadVBO = 0;
    static GLuint s_quadEBO = 0;
    static GLuint s_instanceVBO = 0;
    
    static std::unique_ptr<ShaderVariant> s_shader = nullptr;
    
    // Вспомогательная функция поиска/создания батча с учетом текстуры и блендинга
    Batch& GetOrCreateBatch(int textureId, BlendMode blendMode)
    {
        for (auto& batch : s_batches)
        {
            if (batch.textureId == textureId && batch.blendMode == blendMode) {
                return batch;
            }
        }
        
        s_batches.push_back({ textureId, blendMode, {} });
        return s_batches.back();
    }
    
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
        
        // Атрибут 5: Направление трасера (vec3)
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), (void*)offsetof(BillboardInstance, direction));
        glVertexAttribDivisor(5, 1);
        
        // Атрибут 6: Тип: биллборд/трасер (float)
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), (void*)offsetof(BillboardInstance, type));
        glVertexAttribDivisor(6, 1);
        
        // Атрибут 7: Цвет (vec4)
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), (void*)offsetof(BillboardInstance, color));
        glVertexAttribDivisor(7, 1);
        
        glBindVertexArray(0);
        
        // 4. Инициализируем шейдер биллбордов
        s_shader = std::make_unique<ShaderVariant>();
        s_shader->init(billboard_vs, billboard_fs);
    }
    
    int PreloadTexture(const std::string& path)
    {
        // Если текстура уже загружена — возвращаем кэшированный ID
        if (s_loadedTextures.count(path)) {
            return s_loadedTextures[path];
        }
        
        int width, height, channels;
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        
        if (!data) {
            std::cerr << "[SpriteRenderer] Не удалось загрузить текстуру: "
            << path << std::endl;
            return 0;
        }
        
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        stbi_image_free(data);
        
        s_loadedTextures[path] = textureId;
        return textureId;
    }
    
    int PreloadEngineTexture(const std::string& filename)
    {
        return PreloadTexture("resources/" + filename);
    }
    
    int PreloadProjectTexture(const std::string& filename)
    {
        auto path = ProjectManager::Get().assetsPath() / filename;
        return PreloadTexture(path.string());
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

void SpriteRenderer::AddBillboard(int textureId, glm::vec3 position, float scale, float zRot, const glm::vec4& color, BlendMode blendMode)
{
    auto& batch = GetOrCreateBatch(textureId, blendMode);
    batch.instances.push_back({ position, scale, zRot, glm::vec3(0.0f), 0.0f, color });
}

void SpriteRenderer::AddAxialBillboard(int textureId, glm::vec3 position, float length, glm::vec3 direction, float width, const glm::vec4& color, BlendMode blendMode)
{
    auto& batch = GetOrCreateBatch(textureId, blendMode);
    batch.instances.push_back({ position, width, length, direction, 1.0f, color });
}

void SpriteRenderer::Flush(glm::mat4 view, glm::mat4 projection)
{
    if (s_batches.empty()) return;
    
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE); // Эффекты не пишут в глубину
    
    s_shader->bind();
    s_shader->setUniform("view", view);
    s_shader->setUniform("projection", projection);
    
    glBindVertexArray(s_quadVAO);
    
    for (auto& batch : s_batches) {
        if (batch.instances.empty()) continue;
        
        // ДИНАМИЧЕСКИЙ БЛЕНДИНГ
        if (batch.blendMode == BlendMode::Additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Выжигание света (искры)
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Обычная прозрачность (дым)
        }
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.textureId);
        
        glBindBuffer(GL_ARRAY_BUFFER, s_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, batch.instances.size() * sizeof(BillboardInstance), batch.instances.data());
        
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(batch.instances.size()));
        batch.instances.clear();
    }
    
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}

void SpriteRenderer::Clear()
{
    s_batches.clear();
}
