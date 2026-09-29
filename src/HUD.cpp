//
//  HUD.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#include "HUD.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "../vendor/stb_truetype.h"

#include <iostream>
#include <fstream>

const char* vertexSource = R"(
    #version 410 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoords;
    layout (location = 2) in vec4 aColor;

    out vec2 TexCoords;
    out vec4 Color;

    uniform mat4 uProjection;

    void main() {
        TexCoords = aTexCoords;
        Color = aColor;
        gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    }
)";

const char* fragmentSource = R"(
    #version 410 core
    in vec2 TexCoords;
    in vec4 Color;

    out vec4 FragColor;

    uniform sampler2D uTexture;
    uniform int uMode; // 0 = Цвет, 1 = RGBA Текстура, 2 = Текст (Монохромный атлас)

    void main() {
        if (uMode == 0) {
            FragColor = Color;
        } else if (uMode == 1) {
            FragColor = texture(uTexture, TexCoords) * Color;
        } else if (uMode == 2) {
            // stb_truetype запекает шрифт в R-канал (или Alpha). Используем его как маску прозрачности.
            float alpha = texture(uTexture, TexCoords).r;
            FragColor = vec4(Color.rgb, Color.a * alpha);
        }
    }
)";

namespace HUD {
    
    // Внутренняя структура для очереди команд отрисовки
    struct DrawCommand {
        int mode;               // 0 - цвет, 1 - текстура, 2 - шрифт
        unsigned int textureID; // ID текстуры (если используется)
        unsigned int indexCount;// Сколько индексов рисовать в этой команде
        unsigned int firstIndex;// С какого индекса в EBO начинать рисовать
    };
    
    // Структура вершины для батчинга
    struct Vertex {
        glm::vec2 position;
        glm::vec2 texCoords;
        glm::vec4 color;
    };
    
    int m_width = 1, m_height = 1;
    
    namespace {
        // Лимиты буфера (при необходимости можно автоматически расширять вектор)
        const size_t MAX_QUADS = 5000;
        const size_t MAX_VERTICES = MAX_QUADS * 4;
        const size_t MAX_INDICES = MAX_QUADS * 6;
        
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLuint shaderProgram = 0;
        GLint uProjectionLoc = -1;
        GLint uModeLoc = -1;
        
        std::vector<Vertex> vertices;
        std::vector<DrawCommand> commands;
        
        // Данные шрифта stb_truetype
        GLuint fontTexture = 0;
        stbtt_bakedchar bakedChars[96]; // Храним символы ASCII от 32 до 127
        float fontPixelHeight = 32.0f;
        int atlasWidth = 512;
        int atlasHeight = 512;
        
        // Вспомогательная функция для добавления/переиспользования команд
        void AddQuadCommand(int mode, unsigned int textureID) {
            // Если массив команд не пуст, и последняя команда имеет Тот же режим и Ту же текстуру,
            // мы просто увеличиваем счетчик индексов в ней, вместо создания новой команды.
            if (!commands.empty()) {
                auto& lastCmd = commands.back();
                if (lastCmd.mode == mode && lastCmd.textureID == textureID) {
                    lastCmd.indexCount += 6;
                    return;
                }
            }
            
            // Иначе создаем новую команду отрисовки
            DrawCommand cmd;
            cmd.mode = mode;
            cmd.textureID = textureID;
            cmd.indexCount = 6;
            cmd.firstIndex = static_cast<unsigned int>((vertices.size() / 4) * 6);
            commands.push_back(cmd);
        }
    }
    
    void Init()
    {
        fontPixelHeight = 32.f;
        
        // 1. Компиляция шейдеров
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertexSource, NULL);
        glCompileShader(vs);
        
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragmentSource, NULL);
        glCompileShader(fs);
        
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vs);
        glAttachShader(shaderProgram, fs);
        glLinkProgram(shaderProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        uProjectionLoc = glGetUniformLocation(shaderProgram, "uProjection");
        uModeLoc = glGetUniformLocation(shaderProgram, "uMode");
        
        // 2. Генерация статического индексного буфера для Квадов (0-1-2, 2-3-0)
        std::vector<unsigned int> indices(MAX_INDICES);
        unsigned int offset = 0;
        for (size_t i = 0; i < MAX_INDICES; i += 6) {
            indices[i + 0] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;
            indices[i + 3] = offset + 2;
            indices[i + 4] = offset + 3;
            indices[i + 5] = offset + 0;
            offset += 4;
        }
        
        // 3. Создание OpenGL объектов
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        
        glBindVertexArray(vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        
        glBindVertexArray(0);
        
        
        // 4. Загрузка и запекание шрифта через stb_truetype
        const std::string& fontPath = "assets/fonts/Ruda/Ruda-Medium.ttf";
        std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[HUD] Не удалось открыть файл шрифта: " << fontPath << std::endl;
            return;
        }
        size_t size = file.tellg();
        std::vector<unsigned char> ttfBuffer(size);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(ttfBuffer.data()), size);
        file.close();
        
        std::vector<unsigned char> bitmap(atlasWidth * atlasHeight);
        
        // Запекаем символы от ASCII 32 (пробел) до 126 в одноканальную текстуру
        int result = stbtt_BakeFontBitmap(ttfBuffer.data(), 0, fontPixelHeight,
                                          bitmap.data(), atlasWidth, atlasHeight,
                                          32, 96, bakedChars);
        if (result <= 0) {
            std::cerr << "[HUD] Ошибка: Шрифт не поместился в текстурный атлас " << atlasWidth << "x" << atlasHeight << std::endl;
        }
        
        // Создаем OpenGL текстуру для шрифта
        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        // Используем GL_RED, так как это OpenGL Core Profile (GL_ALPHA/GL_LUMINANCE устарели)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    void Resize(int width, int height)
    {
        m_width = width;
        m_height = height;
        
        glUseProgram(shaderProgram);
        // Создаем ортографическую проекцию (0,0 в левом верхнем углу экрана)
        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, &projection[0][0]);
    }
    
    int GetScreenWidth() { return m_width; }
    int GetScreenHeight() { return m_height; }
    
    void DrawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        if (vertices.size() >= MAX_VERTICES) return;
        
        AddQuadCommand(0, 0);
        
        vertices.push_back({ pos,                               glm::vec2(0.0f, 0.0f), color });
        vertices.push_back({ glm::vec2(pos.x + size.x, pos.y),  glm::vec2(1.0f, 0.0f), color });
        vertices.push_back({ pos + size,                        glm::vec2(1.0f, 1.0f), color });
        vertices.push_back({ glm::vec2(pos.x, pos.y + size.y),  glm::vec2(0.0f, 1.0f), color });
    }
    
    void DrawTexture(const glm::vec2& pos, const glm::vec2& size, unsigned int textureID, const glm::vec4& color)
    {
        if (vertices.size() >= MAX_VERTICES) return;
        
        AddQuadCommand(1, textureID);
        
        vertices.push_back({ pos,                               glm::vec2(0.0f, 0.0f), color });
        vertices.push_back({ glm::vec2(pos.x + size.x, pos.y),  glm::vec2(1.0f, 0.0f), color });
        vertices.push_back({ pos + size,                        glm::vec2(1.0f, 1.0f), color });
        vertices.push_back({ glm::vec2(pos.x, pos.y + size.y),  glm::vec2(0.0f, 1.0f), color });
    }
    
    void DrawText(const std::string& text, const glm::vec2& pos, float scale, const glm::vec4& color)
    {
        float x = pos.x;
        float y = pos.y;
        
        for (char c : text)
        {
            // Обрабатываем только поддерживаемый диапазон ASCII
            if (c >= 32 && c < 128)
            {
                if (vertices.size() >= MAX_VERTICES) break;
                
                stbtt_aligned_quad q;
                // Функция берет символ, рассчитывает смещения и выдает координаты вершин экрана и UV
                stbtt_GetBakedQuad(bakedChars, atlasWidth, atlasHeight, c - 32, &x, &y, &q, 1);
                
                AddQuadCommand(2, fontTexture);
                
                // Заполняем вершины на основе данных из stbtt_aligned_quad
                // Внимание: инвертируем Y для UV, если экранная система координат (0,0) сверху
                vertices.push_back({ glm::vec2(q.x0, q.y0), glm::vec2(q.s0, q.t0), color });
                vertices.push_back({ glm::vec2(q.x1, q.y0), glm::vec2(q.s1, q.t0), color });
                vertices.push_back({ glm::vec2(q.x1, q.y1), glm::vec2(q.s1, q.t1), color });
                vertices.push_back({ glm::vec2(q.x0, q.y1), glm::vec2(q.s0, q.t1), color });
            }
        }
    }
    
    void Flush()
    {
        if (vertices.empty() || commands.empty()) return;
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glDepthMask(GL_FALSE);      // Отключаем запись в буфер глубины
        glDisable(GL_DEPTH_TEST);   // Отключаем чтение буфера глубины
        
        // 1. Загружаем ОДИН РАЗ все вершины кадра на GPU
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());
        glUseProgram(shaderProgram);
        glBindVertexArray(vao);
        
        // 2. Проходимся по очереди команд и выполняем их
        unsigned int activeTexture = 0;
        int activeMode = -1;
        
        for (const auto& cmd : commands)
        {
            // Меняем юниформ режима, только если он изменился
            if (activeMode != cmd.mode) {
                glUniform1i(uModeLoc, cmd.mode);
                activeMode = cmd.mode;
            }
            
            // Биндим текстуру, только если режим текстурный/шрифтовой и текстура сменилась
            if (cmd.mode > 0 && activeTexture != cmd.textureID) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cmd.textureID);
                activeTexture = cmd.textureID;
            }
            
            // Вызов отрисовки конкретного диапазона индексов
            glDrawElements(GL_TRIANGLES, cmd.indexCount, GL_UNSIGNED_INT,reinterpret_cast<void*>(cmd.firstIndex * sizeof(unsigned int)));
        }
        
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        
        // Очищаем буферы для следующего кадра
        
        vertices.clear();
        commands.clear();
    }
    
    void Shutdown()
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (shaderProgram) glDeleteProgram(shaderProgram);
        if (fontTexture) glDeleteTextures(1, &fontTexture);
    }
}
