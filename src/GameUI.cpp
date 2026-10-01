//
//  HUD.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#include "GameUI.h"

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
        Color.r = pow(Color.r, 2.2);
        Color.g = pow(Color.g, 2.2);
        Color.b = pow(Color.b, 2.2);
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

namespace GameUI {
    
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
    
    int currentWidth = 1, currentHeight = 1;
    
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
        float fontPixelHeight = 46.0f;
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
    
    namespace {
        bool inLayoutGroup = false;
        glm::vec2 layoutCursor(0.0f);
        float layoutSpacing = 0.0f;
    }
    
    // Вспомогательная структура для возврата результата расчета
    struct CalculatedRect {
        glm::vec2 position;
        glm::vec2 size;
    };
    
    CalculatedRect CalculateRect(const Layout& layout)
    {
        float screenW = static_cast<float>(currentWidth);
        float screenH = static_cast<float>(currentHeight);
        
        // 1. Вычисляем абсолютные координаты опорных точек якорей на экране
        float anchorLeft   = layout.anchorMin.x * screenW;
        float anchorTop    = layout.anchorMin.y * screenH;
        float anchorRight  = layout.anchorMax.x * screenW;
        float anchorBottom = layout.anchorMax.y * screenH;
        
        CalculatedRect rect;
        
        if (layout.anchorMin == layout.anchorMax) {
            // РЕЖИМ FIXED: Якоря в одной точке.
            // Margin Left/Top — это смещение. Right/Bottom — это Ширина/Высота.
            rect.size.x = layout.marginRight;
            rect.size.y = layout.marginBottom;
            
            // Базовая позиция до учета Pivot
            rect.position.x = anchorLeft + layout.marginLeft;
            rect.position.y = anchorTop + layout.marginTop;
        }
        else {
            // РЕЖИМ STRETCH: Якоря разнесены.
            // Margins — это чистые отступы со всех сторон.
            rect.position.x = anchorLeft + layout.marginLeft;
            rect.position.y = anchorTop + layout.marginTop;
            
            rect.size.x = (anchorRight - layout.marginRight) - rect.position.x;
            rect.size.y = (anchorBottom - layout.marginBottom) - rect.position.y;
        }
        
        // 2. Применяем Pivot (сдвиг origin самого элемента)
        rect.position.x -= layout.pivot.x * rect.size.x;
        rect.position.y -= layout.pivot.y * rect.size.y;
        
        // Защита от "мыла": округляем до целых пикселей
        rect.position.x = std::round(rect.position.x);
        rect.position.y = std::round(rect.position.y);
        rect.size.x = std::round(rect.size.x);
        rect.size.y = std::round(rect.size.y);
        
        return rect;
    }
    
    // Вспомогательная функция для безопасного сравнения float-векторов (защита от погрешностей)
    bool IsEpsilonEqual(const glm::vec2& a, const glm::vec2& b) {
        return std::abs(a.x - b.x) < 0.0001f && std::abs(a.y - b.y) < 0.0001f;
    }
    
    CalculatedRect CalculateRect(const Layout& layout, const glm::vec2& fixedSize) {
        float screenW = static_cast<float>(currentWidth);
        float screenH = static_cast<float>(currentHeight);
        
        CalculatedRect rect;
        
        // Если min и max якоря совпадают — это фиксированный размер (Fixed)
        if (IsEpsilonEqual(layout.anchorMin, layout.anchorMax)) {
            rect.size = fixedSize;
            
            // Базовое положение от точки якоря + смещение (marginLeft/marginTop работают как offsetX/Y)
            float anchorX = layout.anchorMin.x * screenW;
            float anchorY = layout.anchorMin.y * screenH;
            rect.position.x = anchorX + layout.marginLeft;
            rect.position.y = anchorY + layout.marginTop;
        }
        // Если разнесены — это режим растягивания (Stretch)
        else {
            float anchorLeft   = layout.anchorMin.x * screenW;
            float anchorTop    = layout.anchorMin.y * screenH;
            float anchorRight  = layout.anchorMax.x * screenW;
            float anchorBottom = layout.anchorMax.y * screenH;
            
            rect.position.x = anchorLeft + layout.marginLeft;
            rect.position.y = anchorTop + layout.marginTop;
            
            rect.size.x = (anchorRight - layout.marginRight) - rect.position.x;
            rect.size.y = (anchorBottom - layout.marginBottom) - rect.position.y;
        }
        
        // Применяем Pivot (смещение относительно опорной точки самого элемента)
        rect.position.x -= layout.pivot.x * rect.size.x;
        rect.position.y -= layout.pivot.y * rect.size.y;
        
        // Округляем до целых пикселей, чтобы избежать "мыла" при субпиксельном рендеринге
        rect.position.x = std::round(rect.position.x);
        rect.position.y = std::round(rect.position.y);
        rect.size.x = std::round(rect.size.x);
        rect.size.y = std::round(rect.size.y);
        
        return rect;
    }
    
    void Init()
    {
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
        const std::string& fontPath = "assets/fonts/Georgia/SdGeorgiaLight.ttf";
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    void Resize(int width, int height)
    {
        currentWidth = width;
        currentHeight = height;
        
        glUseProgram(shaderProgram);
        // Создаем ортографическую проекцию (0,0 в левом верхнем углу экрана)
        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(currentWidth), static_cast<float>(currentHeight), 0.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, &projection[0][0]);
    }
    
    int GetScreenWidth() { return currentWidth; }
    int GetScreenHeight() { return currentHeight; }
    
    // Вычисление ширины строки в пикселях без генерации геометрии
    float GetTextWidth(const std::string& text) {
        float width = 0.0f;
        float dummyY = 0.0f;
        
        for (char c : text) {
            if (c >= 32 && c < 128) {
                stbtt_aligned_quad q;
                // Передаем dummyY, а вместо x передаем нашу переменную width.
                // Функция сама сдвинет width на нужное расстояние (advance width) для следующего символа.
                stbtt_GetBakedQuad(bakedChars, atlasWidth, atlasHeight, c - 32, &width, &dummyY, &q, 1);
            }
        }
        return width;
    }
    
    void DrawRect(const Layout& layout, const glm::vec4& color)
    {
        if (vertices.size() >= MAX_VERTICES) return;
        
        // Вычисляем финальные экранные координаты и размеры
        CalculatedRect rect = CalculateRect(layout);
        
        AddQuadCommand(0, 0);
        vertices.push_back({ rect.position,                                             glm::vec2(0.0f, 0.0f), color });
        vertices.push_back({ glm::vec2(rect.position.x + rect.size.x, rect.position.y), glm::vec2(1.0f, 0.0f), color });
        vertices.push_back({ rect.position + rect.size,                                 glm::vec2(1.0f, 1.0f), color });
        vertices.push_back({ glm::vec2(rect.position.x, rect.position.y + rect.size.y), glm::vec2(0.0f, 1.0f), color });
    }
    
    void DrawTexture(const Layout& layout, unsigned int texture, const glm::vec4& color)
    {
        if (vertices.size() >= MAX_VERTICES) return;
        
        // Вычисляем финальные экранные координаты и размеры
        CalculatedRect rect = CalculateRect(layout);
        
        AddQuadCommand(1, texture);
        vertices.push_back({ rect.position,                                             glm::vec2(0.0f, 0.0f), color });
        vertices.push_back({ glm::vec2(rect.position.x + rect.size.x, rect.position.y), glm::vec2(1.0f, 0.0f), color });
        vertices.push_back({ rect.position + rect.size,                                 glm::vec2(1.0f, 1.0f), color });
        vertices.push_back({ glm::vec2(rect.position.x, rect.position.y + rect.size.y), glm::vec2(0.0f, 1.0f), color });
    }
    
    void GenerateTextQuads(const std::string& text, const glm::vec2& pos, const glm::vec4& color)
    {
        // 3. Генерация геометрии символов с помощью stb_truetype
        float x = pos.x;
        
        // ВАЖНО: stb_truetype считает координату Y от базовой линии шрифта (baseline).
        // Наша rect.position.y указывает на левый ВЕРХНИЙ угол текстового блока.
        // Чтобы текст не улетал вверх за границы блока, мы смещаем стартовую Y вниз на высоту шрифта.
        float y = pos.y + fontPixelHeight;
        
        for (char c : text) {
            // Ограничиваем рендеринг поддерживаемым диапазоном ASCII
            if (c >= 32 && c < 128) {
                if (vertices.size() >= MAX_VERTICES) {
                    // Если буфер переполнен, мы вынуждены прерваться.
                    // (Поскольку Flush() посреди кадра вызывать нельзя, мы просто прекращаем отрисовку оставшихся символов)
                    break;
                }
                
                stbtt_aligned_quad q;
                // Запрашиваем у stb_truetype расчет текстурных (UV) и экранных координат для символа.
                // Переменные x и y передаются по указателю, функция автоматически сдвинет x вправо для следующего символа.
                stbtt_GetBakedQuad(bakedChars, atlasWidth, atlasHeight, c - 32, &x, &y, &q, 1);
                
                // Добавляем команду отрисовки шрифта в очередь (система сама объединит буквы в один батч)
                AddQuadCommand(2, fontTexture);
                
                // Заполняем вершины квада
                vertices.push_back({ glm::vec2(q.x0, q.y0), glm::vec2(q.s0, q.t0), color });
                vertices.push_back({ glm::vec2(q.x1, q.y0), glm::vec2(q.s1, q.t0), color });
                vertices.push_back({ glm::vec2(q.x1, q.y1), glm::vec2(q.s1, q.t1), color });
                vertices.push_back({ glm::vec2(q.x0, q.y1), glm::vec2(q.s0, q.t1), color });
            }
        }
    }
    
    void DrawText(const Layout& layout, const std::string& text, const glm::vec4& color, bool drawShadow, const glm::vec4& shadowColor)
    {
        if (text.empty()) return;
        
        // 1. Вычисляем физический размер строки текста в пикселях
        float textWidth = GetTextWidth(text);
        float textHeight = fontPixelHeight; // Высота шрифта из stb_truetype
        glm::vec2 textSize(textWidth, textHeight);
        
        // 2. Рассчитываем позицию текстового блока на экране через универсальный CalculateRect
        // Для текста мы ВСЕГДА передаем его реальный физический размер как fixedSize
        CalculatedRect rect = CalculateRect(layout, textSize);
        
//        // Если мы находимся внутри Layout-группы (вертикальный/горизонтальный стек)
//        if (currentLayoutDir != LayoutDirection::None) {
//            rect.position = layoutCursor;
//            
//            // Корректируем позицию внутри стека на основе Pivot (например, если текст центрирован)
//            rect.position.x -= layout.pivot.x * rect.size.x;
//            rect.position.y -= layout.pivot.y * rect.size.y;
//            
//            // Сдвигаем курсор контейнера для следующего элемента
//            if (currentLayoutDir == LayoutDirection::Vertical) {
//                layoutCursor.y += rect.size.y + layoutSpacing;
//            } else if (currentLayoutDir == LayoutDirection::Horizontal) {
//                layoutCursor.x += rect.size.x + layoutSpacing;
//            }
//        }
        
        if (drawShadow)
        {
            glm::vec2 shadowOffset = {2.0f, 2.0f};
            GenerateTextQuads(text, rect.position + shadowOffset, shadowColor);
        }
        
        // 2. ЗАТЕМ генерируем квады для ОСНОВНОГО ТЕКСТА поверх
        GenerateTextQuads(text, rect.position, color);
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

namespace GameUI {
    
    // 1. Фиксированный элемент в левом верхнем углу
    Layout Layout::TopLeft(float x, float y, float width, float height)
    {
        Layout p;
        p.anchorMin = p.anchorMax = glm::vec2(0.0f, 0.0f);
        p.pivot = glm::vec2(0.0f, 0.0f);
        p.marginLeft = x;
        p.marginTop = y;
        p.marginRight = width;
        p.marginBottom = height;
        return p;
    }
    
    // 2. Фиксированный элемент в правом верхнем углу (с автоматическим пивотом)
    Layout Layout::TopRight(float x, float y, float width, float height)
    {
        Layout p;
        p.anchorMin = p.anchorMax = glm::vec2(1.0f, 0.0f);
        p.pivot = glm::vec2(1.0f, 0.0f);
        p.marginLeft = -x;
        p.marginTop = y;
        p.marginRight = width;
        p.marginBottom = height;
        return p;
    }
    
    Layout Layout::BottomLeft(float x, float y, float width, float height)
    {
        Layout p;
        p.anchorMin = p.anchorMax = glm::vec2(0.0f, 1.0f);
        p.pivot = glm::vec2(0.0f, 1.0f);
        p.marginLeft = x;
        p.marginTop = -y;
        p.marginRight = width;
        p.marginBottom = height;
        return p;
    }
    
    // 2. Фиксированный элемент в правом верхнем углу (с автоматическим пивотом)
    Layout Layout::BottomRight(float x, float y, float width, float height)
    {
        Layout p;
        p.anchorMin = p.anchorMax = glm::vec2(1.0f, 1.0f);
        p.pivot = glm::vec2(1.0f, 1.0f);
        p.marginLeft = -x;
        p.marginTop = -y;
        p.marginRight = width;
        p.marginBottom = height;
        return p;
    }
    
    // 3. Элемент строго по центру экрана
    Layout Layout::Center(float width, float height, glm::vec2 offset)
    {
        Layout p;
        p.anchorMin = p.anchorMax = glm::vec2(0.5f, 0.5f);
        p.pivot = glm::vec2(0.5f, 0.5f); // Центрируем сам элемент
        p.marginLeft = offset.x;
        p.marginTop = offset.y;
        p.marginRight = width;
        p.marginBottom = height;
        return p;
    }
    
    // 4. Полное растягивание на весь экран
    Layout Layout::FullScreen(float margin)
    {
        Layout p;
        p.anchorMin = glm::vec2(0.0f, 0.0f);
        p.anchorMax = glm::vec2(1.0f, 1.0f);
        p.marginLeft = p.marginTop = p.marginRight = p.marginBottom = margin;
        return p;
    }
}
