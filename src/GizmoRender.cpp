//
//  DebugRenderer.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 02.12.24.
//

#include "GizmoRender.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <cmath>

// Включаем реализацию stb_truetype только в этом файле
#define STB_TRUETYPE_IMPLEMENTATION
#include "../vendor/stb_truetype.h"

// --- ШЕЙДЕРЫ ДЛЯ ЛИНИЙ ---
const char* lineVS = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec4 aColor;
    out vec4 vColor;
    uniform mat4 uViewProj;
    void main() {
        vColor = aColor;
        gl_Position = uViewProj * vec4(aPos, 1.0);
    }
)";

const char* lineFS = R"(
    #version 410 core
    in vec4 vColor;
    out vec4 FragColor;
    void main() {
        FragColor = vColor;
    }
)";

// --- ШЕЙДЕРЫ ДЛЯ ТЕКСТА И ИКОНОК ---
const char* texVS = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aUV;
    layout (location = 2) in vec4 aColor;
    out vec2 vUV;
    out vec4 vColor;
    uniform mat4 uViewProj;
    void main() {
        vUV = aUV;
        vColor = aColor;
        gl_Position = uViewProj * vec4(aPos, 1.0);
    }
)";

const char* texFS = R"(
    #version 410 core
    in vec2 vUV;
    in vec4 vColor;
    out vec4 FragColor;
    uniform sampler2D uAtlas;
    void main() {
        vec4 texElement = texture(uAtlas, vUV);
        // Атлас содержит: R-канал для маски шрифта, G-канал для иконок
        // Если это пиксель шрифта (используем R), если иконка — берем как полноценный цвет или маску
        float alpha = texElement.a; 
        if(alpha < 0.1) discard;
        
        FragColor = vec4(vColor.rgb * texElement.rgb, vColor.a * alpha);
    }
)";


// ======================== GRID =================================
#include "ShaderVariant.h"
//#include "RenderDevice.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

class Grid
{
public:
    void init();
    void draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
    
private:
    VertexBuffer* posBuffer;
    IndexBuffer* indexBuffer;
    VertexArray* vertexArray;
};

static ShaderVariant s_grid_shader;

void Grid::init()
{
    float positions[] = {
        -512.0f, 0.0f, -512.0f,
        -512.0f, 0.0f,  512.0f,
        512.0f, 0.0f,  512.0f,
        512.0f, 0.0f, -512.0f,
    };
    
    uint32_t indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    vertexArray = new VertexArray();
    
    posBuffer = new VertexBuffer(positions, sizeof(positions));
    posBuffer->setLayout({ { VertexAttributeLocation::Position, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(posBuffer);
    
    indexBuffer = new IndexBuffer(indices, 6);
    vertexArray->setIndexBuffer(indexBuffer);
    
    s_grid_shader.init("resources/shaders/grid.glsl");
}

void Grid::draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const
{
    s_grid_shader.bind();
    s_grid_shader.setUniform("uVP", projectionMatrix * viewMatrix);
    
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDepthMask(GL_FALSE);
    
    glBindVertexArray(vertexArray->getID());
    glDrawElements(GL_TRIANGLES, indexBuffer->getCount(), GL_UNSIGNED_INT, (void *)0);
    
    s_grid_shader.unbind();
    
    glDepthMask(GL_TRUE);
}

static Grid g_grid;
// ======================== GRID =================================

void GizmoRender::Initialize()
{
    CompileShaders();
    
    // 1. Создание буферов для Линий
    glGenVertexArrays(1, &m_LineVAO);
    glGenBuffers(1, &m_LineVBO);
    glBindVertexArray(m_LineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_LINE_VERTICES * sizeof(LineVertex), nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, color));
    
    // 2. Создание буферов для Текстур (Текст/Иконки)
    glGenVertexArrays(1, &m_TexVAO);
    glGenBuffers(1, &m_TexVBO);
    glBindVertexArray(m_TexVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_TexVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TEXT_VERTICES * sizeof(TextureVertex), nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), (void*)offsetof(TextureVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), (void*)offsetof(TextureVertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextureVertex), (void*)offsetof(TextureVertex, color));
    
    glBindVertexArray(0);
    
    // 3. Генерация Встроенного Атласа (Шрифт + Процедурная Иконка)
    GenerateAtlas("resources/fonts/Ruda/Ruda-Medium.ttf");
    
    g_grid.init();
}

void GizmoRender::GenerateAtlas(const std::string& fontPath)
{
    const int atlasWidth = 512;
    const int atlasHeight = 512;
    
    // Создаем пустой буфер RGBA для атласа
    std::vector<unsigned char> atlasData(atlasWidth * atlasHeight * 4, 0);
    
    // --- Часть А: Рендеринг шрифта через stb_truetype ---
    FILE* fontFile = fopen(fontPath.c_str(), "rb");
    if (!fontFile) {
        std::cerr << "[GizmoRender] Не удалось открыть файл шрифта: " << fontPath << ". Текст не будет отображаться!\n";
    } else {
        // Читаем файл шрифта в память
        fseek(fontFile, 0, SEEK_END);
        long size = ftell(fontFile);
        fseek(fontFile, 0, SEEK_SET);
        std::vector<unsigned char> fontBuffer(size);
        fread(fontBuffer.data(), 1, size, fontFile);
        fclose(fontFile);
        
        stbtt_fontinfo font;
        if (stbtt_InitFont(&font, fontBuffer.data(), 0)) {
            float fontHeightPixels = 32.0f; // Базовое разрешение букв в атласе
            float scale = stbtt_ScaleForPixelHeight(&font, fontHeightPixels);
            
            int ascent, descent, lineGap;
            stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
            
            int currentX = 0;
            int currentY = 0;
            int maxHeightInRow = 0;
            
            // Запекаем первые 128 ASCII символов
            for (int i = 32; i < 128; ++i) {
                int advance, lsb;
                stbtt_GetCodepointHMetrics(&font, i, &advance, &lsb);
                
                int x1, y1, x2, y2;
                stbtt_GetCodepointBitmapBox(&font, i, scale, scale, &x1, &y1, &x2, &y2);
                
                int chW = x2 - x1;
                int chH = y2 - y1;
                
                if (currentX + chW >= atlasWidth) {
                    currentX = 0;
                    currentY += maxHeightInRow + 2;
                    maxHeightInRow = 0;
                }
                
                if (chW > 0 && chH > 0) {
                    std::vector<unsigned char> monoBitmap(chW * chH);
                    stbtt_MakeCodepointBitmap(&font, monoBitmap.data(), chW, chH, chW, scale, scale, i);
                    
                    // Копируем монохромную маску в R-канал нашего RGBA атласа
                    for (int y = 0; y < chH; ++y) {
                        for (int x = 0; x < chW; ++x) {
                            int atlasIdx = ((currentY + y) * atlasWidth + (currentX + x)) * 4;
                            unsigned char pixelVal = monoBitmap[y * chW + x];
                            atlasData[atlasIdx + 0] = 255;      // R (используем белый цвет для шрифта)
                            atlasData[atlasIdx + 1] = 255;      // G
                            atlasData[atlasIdx + 2] = 255;      // B
                            atlasData[atlasIdx + 3] = pixelVal; // A
                        }
                    }
                }
                
                // Сохраняем UV и метрики символа
                m_Glyphs[i].uvMin = glm::vec2((float)currentX / atlasWidth, (float)currentY / atlasHeight);
                m_Glyphs[i].uvMax = glm::vec2((float)(currentX + chW) / atlasWidth, (float)(currentY + chH) / atlasHeight);
                m_Glyphs[i].size = glm::vec2((float)chW, (float)chH);
                m_Glyphs[i].bearing = glm::vec2((float)x1, (float)-y1);
                m_Glyphs[i].advance = (float)advance * scale;
                
                currentX += chW + 2;
                if (chH > maxHeightInRow) maxHeightInRow = chH;
            }
        }
    }
    
    // --- Часть Б: Рисуем процедурную Дебаг-Иконку (Лампочка/Точка внимания) в нижнем правом углу атласа ---
    // Выделим под иконку квадрат 64x64 в координатах атласа [448, 448] до [512, 512]
    int iconX = 448, iconY = 448, iconSize = 64;
    for (int y = 0; y < iconSize; ++y) {
        for (int x = 0; x < iconSize; ++x) {
            float dx = (x - iconSize / 2.0f) / (iconSize / 2.0f);
            float dy = (y - iconSize / 2.0f) / (iconSize / 2.0f);
            float dist = std::sqrt(dx*dx + dy*dy);
            
            int atlasIdx = ((iconY + y) * atlasWidth + (iconX + x)) * 4;
            if (dist < 0.8f) {
                // Рисуем внутренний круг (например, желтый свет)
                atlasData[atlasIdx + 0] = 255; // R
                atlasData[atlasIdx + 1] = 230; // G
                atlasData[atlasIdx + 2] = 50;  // B
                atlasData[atlasIdx + 3] = 255; // A
            } else if (dist < 0.95f) {
                // Черная обводка
                atlasData[atlasIdx + 0] = 0; atlasData[atlasIdx + 1] = 0; atlasData[atlasIdx + 2] = 0; atlasData[atlasIdx + 3] = 255;
            }
            // Все остальное остается прозрачным (0)
        }
    }
    // Сохраняем жестко заданные UV-координаты для нашей иконки
    m_IconUVMin = glm::vec2((float)iconX / atlasWidth, (float)iconY / atlasHeight);
    m_IconUVMax = glm::vec2((float)(iconX + iconSize) / atlasWidth, (float)(iconY + iconSize) / atlasHeight);
    
    // --- Создание текстуры в OpenGL ---
    glGenTextures(1, &m_AtlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GizmoRender::CompileShaders()
{
    auto compile = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    };
    
    // Сборка шейдера линий
    GLuint lvs = compile(GL_VERTEX_SHADER, lineVS);
    GLuint lfs = compile(GL_FRAGMENT_SHADER, lineFS);
    m_LineShader = glCreateProgram();
    glAttachShader(m_LineShader, lvs); glAttachShader(m_LineShader, lfs);
    glLinkProgram(m_LineShader);
    glDeleteShader(lvs); glDeleteShader(lfs);
    
    // Сборка шейдера текстур
    GLuint tvs = compile(GL_VERTEX_SHADER, texVS);
    GLuint tfs = compile(GL_FRAGMENT_SHADER, texFS);
    m_TexShader = glCreateProgram();
    glAttachShader(m_TexShader, tvs); glAttachShader(m_TexShader, tfs);
    glLinkProgram(m_TexShader);
    glDeleteShader(tvs); glDeleteShader(tfs);
}

void GizmoRender::Clear() {
    m_LineVertexCount = 0;
    m_TexVertexCount = 0;
}

void GizmoRender::Flush(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
{
    glm::mat4 viewProj = projectionMatrix * viewMatrix;
    
    // Извлекаем векторы направления камеры для Биллбординга из инвертированной матрицы вида
    glm::mat4 invView = glm::inverse(viewMatrix);
    m_CamRight = glm::vec3(invView[0]);
    m_CamUp    = glm::vec3(invView[1]);
    
    // --- Рендеринг ЛИНИЙ ---
    if (m_LineVertexCount > 0)
    {
        glUseProgram(m_LineShader);
        glUniformMatrix4fv(glGetUniformLocation(m_LineShader, "uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
        glBindVertexArray(m_LineVAO);glBindBuffer(GL_ARRAY_BUFFER, m_LineVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_LineVertexCount * sizeof(LineVertex), m_LineBuffer.data());glDrawArrays(GL_LINES, 0, m_LineVertexCount);
    }
    
    // --- Рендеринг ТЕКСТА И ИКОНОК ---
    if (m_TexVertexCount > 0)
    {
        glUseProgram(m_TexShader);
        glUniformMatrix4fv(glGetUniformLocation(m_TexShader, "uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_AtlasTexture);
        glUniform1i(glGetUniformLocation(m_TexShader, "uAtlas"), 0);
        glBindVertexArray(m_TexVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_TexVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_TexVertexCount * sizeof(TextureVertex), m_TexBuffer.data());
        glDrawArrays(GL_TRIANGLES, 0, m_TexVertexCount);
        glDisable(GL_BLEND);
    }
    
    glBindVertexArray(0);
    glUseProgram(0);
}

// --- ФУНКЦИИ РИСОВАНИЯ ГЕОМЕТРИИ ---
void GizmoRender::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
{
    if (m_LineVertexCount + 2 > MAX_LINE_VERTICES) return;
    
    m_LineBuffer[m_LineVertexCount++] = { start, color };
    m_LineBuffer[m_LineVertexCount++] = { end, color };}

void GizmoRender::DrawAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
{
    glm::vec3 c[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z}
    };
    
    // Основания
    DrawLine(c[0], c[1], color);
    DrawLine(c[1], c[2], color);
    DrawLine(c[2], c[3], color);
    DrawLine(c[3], c[0], color);
    DrawLine(c[4], c[5], color);
    DrawLine(c[5], c[6], color);
    DrawLine(c[6], c[7], color);
    DrawLine(c[7], c[4], color);
    
    // Стойки
    DrawLine(c[0], c[4], color);
    DrawLine(c[1], c[5], color);
    DrawLine(c[2], c[6], color);
    DrawLine(c[3], c[7], color);
}

void GizmoRender::DrawCircle(const glm::vec3& center, float radius, const glm::vec3& normal, const glm::vec4& color, int segments)
{
    glm::vec3 up = (glm::abs(normal.z) < 0.999f) ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    glm::vec3 tangent = glm::normalize(glm::cross(up, normal));glm::vec3 bitangent = glm::cross(normal, tangent);
    float angleStep = glm::two_pi<float>() / segments;
    glm::vec3 prev = center + tangent * radius;
    
    for (int i = 1; i <= segments; ++i) {
        float angle = i * angleStep;glm::vec3 current = center + (tangent * std::cos(angle) + bitangent * std::sin(angle)) * radius;
        DrawLine(prev, current, color);
        prev = current;
    }
}

void GizmoRender::DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments)
{
    DrawCircle(center, radius, glm::vec3(1, 0, 0), color, segments);
    DrawCircle(center, radius, glm::vec3(0, 1, 0), color, segments);
    DrawCircle(center, radius, glm::vec3(0, 0, 1), color, segments);
}

void GizmoRender::DrawCamera(const glm::mat4& modelMatrix, float width, float height, const glm::vec4& color)
{
    float halfW = width * 0.5f;
    float boxSize = height * 0.2f; // Размер корпуса камеры
    
    // Цвета
    glm::vec4 redColor(1.0f, 0.0f, 0.0f, 1.0f); // Красный для стрелки
    
    // ==========================================
    // 1. ЛОКАЛЬНЫЕ КООРДИНАТЫ (Взгляд вдоль -Z)
    // ==========================================
    
    // Пирамида (Вершина в 0, основание смещено на -height)
    glm::vec4 localTop(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 localB0(-halfW, -halfW, -height, 1.0f);
    glm::vec4 localB1( halfW, -halfW, -height, 1.0f);
    glm::vec4 localB2( halfW,  halfW, -height, 1.0f);
    glm::vec4 localB3(-halfW,  halfW, -height, 1.0f);
    
    // Равносторонний треугольник для метки "Верх кадра"
    // Делаем сторону треугольника равной 20% от ширины кадра
    float triSide = width * 0.2f;
    float triHalfSide = triSide * 0.5f;
    // Высота равностороннего треугольника: h = a * sqrt(3) / 2 ≈ a * 0.866
    float triHeight = triSide * 0.866f;
    
    glm::vec4 localTriLeft(-triHalfSide, halfW, -height, 1.0f);        // Левый угол у основания
    glm::vec4 localTriRight(triHalfSide, halfW, -height, 1.0f);       // Правый угол у основания
    glm::vec4 localTriPeak(0.0f, halfW + triHeight, -height, 1.0f);   // Вершина треугольника
    
    // Стрелка направления (Из центра кадра вперед на некоторое расстояние)
    glm::vec4 localArrowStart(0.0f, 0.0f, -height, 1.0f);
    glm::vec4 localArrowEnd(0.0f, 0.0f, -height * 1.5f, 1.0f);
    
    // Наконечник стрелки
    glm::vec4 localArrowLeft(-halfW * 0.2f, 0.0f, -height * 1.35f, 1.0f);
    glm::vec4 localArrowRight(halfW * 0.2f, 0.0f, -height * 1.35f, 1.0f);
    
    // ==========================================
    // 2. ПЕРЕВОД В МИРОВЫЕ КООРДИНАТЫ
    // ==========================================
    glm::vec3 top = glm::vec3(modelMatrix * localTop);
    glm::vec3 b0  = glm::vec3(modelMatrix * localB0);
    glm::vec3 b1  = glm::vec3(modelMatrix * localB1);
    glm::vec3 b2  = glm::vec3(modelMatrix * localB2);
    glm::vec3 b3  = glm::vec3(modelMatrix * localB3);
    
    glm::vec3 triLeft  = glm::vec3(modelMatrix * localTriLeft);
    glm::vec3 triRight = glm::vec3(modelMatrix * localTriRight);
    glm::vec3 triPeak  = glm::vec3(modelMatrix * localTriPeak);
    
    glm::vec3 arrowStart = glm::vec3(modelMatrix * localArrowStart);
    glm::vec3 arrowEnd   = glm::vec3(modelMatrix * localArrowEnd);
    glm::vec3 arrowLeft  = glm::vec3(modelMatrix * localArrowLeft);
    glm::vec3 arrowRight = glm::vec3(modelMatrix * localArrowRight);
    
    // ==========================================
    // 3. ОТРИСОВКА ВСЕХ ЛИНИЙ
    // ==========================================
    
    // --- Пирамида ---
    DrawLine(b0, b1, color); DrawLine(b1, b2, color);
    DrawLine(b2, b3, color); DrawLine(b3, b0, color); // Рамка кадра
    DrawLine(b0, top, color); DrawLine(b1, top, color);
    DrawLine(b2, top, color); DrawLine(b3, top, color); // Боковые лучи
    
    // --- Метка верха (Равносторонний треугольник) ---
    DrawLine(triLeft, triRight, color); // Основание треугольника на рамке
    DrawLine(triLeft, triPeak, color);  // Левая грань
    DrawLine(triRight, triPeak, color); // Правая грань
    
    // --- Стрелка направления взгляда (Красная) ---
    DrawLine(arrowStart, arrowEnd, redColor);    // Стержень стрелки
    DrawLine(arrowEnd, arrowLeft, redColor);     // Левый наконечник
    DrawLine(arrowEnd, arrowRight, redColor);    // Правый наконечник
}

// --- ФУНКЦИИ РИСОВАНИЯ БИЛЛБОРДОВ (ИКОНКИ И ТЕКСТ) ---
void GizmoRender::DrawIcon3D(const glm::vec3& worldPos, const glm::vec2& scale, const glm::vec4& color)
{
    if (m_TexVertexCount + 6 > MAX_TEXT_VERTICES) return;
    
    // Считаем 4 угла квада, ориентированного на камеру
    float halfW = scale.x * 0.5f;
    float halfH = scale.y * 0.5f;
    glm::vec3 p0 = worldPos - m_CamRight * halfW - m_CamUp * halfH;
    glm::vec3 p1 = worldPos + m_CamRight * halfW - m_CamUp * halfH;
    glm::vec3 p2 = worldPos + m_CamRight * halfW + m_CamUp * halfH;
    glm::vec3 p3 = worldPos - m_CamRight * halfW + m_CamUp * halfH;
    
    // Треугольник 1
    m_TexBuffer[m_TexVertexCount++] = { p0, {m_IconUVMin.x, m_IconUVMax.y}, color };
    m_TexBuffer[m_TexVertexCount++] = { p1, {m_IconUVMax.x, m_IconUVMax.y}, color };
    m_TexBuffer[m_TexVertexCount++] = { p2, {m_IconUVMax.x, m_IconUVMin.y}, color };
    
    // Треугольник 2
    m_TexBuffer[m_TexVertexCount++] = { p0, {m_IconUVMin.x, m_IconUVMax.y}, color };
    m_TexBuffer[m_TexVertexCount++] = { p2, {m_IconUVMax.x, m_IconUVMin.y}, color };
    m_TexBuffer[m_TexVertexCount++] = { p3, {m_IconUVMin.x, m_IconUVMin.y}, color };
    
}

void GizmoRender::DrawText3D(const glm::vec3& worldPos, const std::string& text, float scale, const glm::vec4& color)
{
    glm::vec3 cursor = worldPos;
    
    // Коэффициент уменьшения экранного шрифта для 3D пространства
    float spaceScale = scale * 0.01f;
    
    for (char c : text)
    {
        if (c < 32 || c >= 128) continue;
        
        const GlyphInfo& glyph = m_Glyphs[c];
        
        if (glyph.size.x > 0 && glyph.size.y > 0)
        {
            if (m_TexVertexCount + 6 > MAX_TEXT_VERTICES) return;
            
            // Смещение символа относительно базовой линии и курсора в 3D
            glm::vec3 characterOrigin = cursor + m_CamRight * (glyph.bearing.x * spaceScale) - m_CamUp * ((glyph.size.y - glyph.bearing.y) * spaceScale);
            float w = glyph.size.x * spaceScale;
            float h = glyph.size.y * spaceScale;glm::vec3 p0 = characterOrigin;
            
            glm::vec3 p1 = characterOrigin + m_CamRight * w;
            glm::vec3 p2 = characterOrigin + m_CamRight * w + m_CamUp * h;
            glm::vec3 p3 = characterOrigin + m_CamUp * h;
            
            // Квад символа (Аналогично иконке, но со своими UV)
            m_TexBuffer[m_TexVertexCount++] = { p0, {glyph.uvMin.x, glyph.uvMax.y}, color };
            m_TexBuffer[m_TexVertexCount++] = { p1, {glyph.uvMax.x, glyph.uvMax.y}, color };
            m_TexBuffer[m_TexVertexCount++] = { p2, {glyph.uvMax.x, glyph.uvMin.y}, color };
            m_TexBuffer[m_TexVertexCount++] = { p0, {glyph.uvMin.x, glyph.uvMax.y}, color };
            m_TexBuffer[m_TexVertexCount++] = { p2, {glyph.uvMax.x, glyph.uvMin.y}, color };
            m_TexBuffer[m_TexVertexCount++] = { p3, {glyph.uvMin.x, glyph.uvMin.y}, color };
        }
        
        // Перемещаем курсор дальше по строке
        cursor += m_CamRight * (glyph.advance * spaceScale);
    }
}

void GizmoRender::Shutdown()
{
    glDeleteVertexArrays(1, &m_LineVAO);
    glDeleteBuffers(1, &m_LineVBO);
    glDeleteProgram(m_LineShader);
    glDeleteVertexArrays(1, &m_TexVAO);
    glDeleteBuffers(1, &m_TexVBO);
    glDeleteProgram(m_TexShader);
    glDeleteTextures(1, &m_AtlasTexture);
}

void GizmoRender::DrawGrid(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix)
{
    g_grid.draw(viewMatrix, projectionMatrix);
}
