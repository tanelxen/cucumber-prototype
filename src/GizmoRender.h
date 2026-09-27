//
//  DebugRenderer.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 02.12.24.
//

#pragma once

#include <glm/glm.hpp>
#include <array>
#include <string>

class GizmoRender
{
public:
    static GizmoRender& Get()
    {
        static GizmoRender instance;
        return instance;
    }
    
    void Initialize();
    void Shutdown();
    void Clear();
    void Flush(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    // Отрисовка геометрии (теперь на массивах)
    void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
    void DrawAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);
    void DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments = 16);
    void DrawCircle(const glm::vec3& center, float radius, const glm::vec3& normal, const glm::vec4& color, int segments = 32);
    void DrawCamera(const glm::mat4& modelMatrix, float width, float height, const glm::vec4& color);
    
    // Новое: Отрисовка иконок и текста в 3D пространстве (Биллборды)
    void DrawIcon3D(const glm::vec3& worldPos, const glm::vec2& scale, const glm::vec4& color);
    void DrawText3D(const glm::vec3& worldPos, const std::string& text, float scale, const glm::vec4& color);
    
private:
    GizmoRender() = default;
    
    // Константы размера буферов
    static constexpr unsigned int MAX_LINES = 50000;
    static constexpr unsigned int MAX_LINE_VERTICES = MAX_LINES * 2;
    
    static constexpr unsigned int MAX_QUADS = 5000;
    static constexpr unsigned int MAX_TEXT_VERTICES = MAX_QUADS * 6; // 6 вершин на квад (без EBO для простоты батча)
    
    struct LineVertex {
        glm::vec3 position;
        glm::vec4 color;
    };
    
    struct TextureVertex
    {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec4 color;
    };
    
    // Буфер линий
    unsigned int m_LineVAO = 0, m_LineVBO = 0, m_LineShader = 0;
    std::array<LineVertex, MAX_LINE_VERTICES> m_LineBuffer;
    unsigned int m_LineVertexCount = 0;
    
    // Буфер текста/иконок
    unsigned int m_TexVAO = 0, m_TexVBO = 0, m_TexShader = 0;
    std::array<TextureVertex, MAX_TEXT_VERTICES> m_TexBuffer;
    unsigned int m_TexVertexCount = 0;
    
    unsigned int m_AtlasTexture = 0;
    
    void GenerateAtlas(const std::string& fontPath);
    void CompileShaders();
    
    
    struct GlyphInfo {
        glm::vec2 uvMin;
        glm::vec2 uvMax;
        glm::vec2 size;
        glm::vec2 bearing;
        float advance;
    };
    
    std::array<GlyphInfo, 128> m_Glyphs; // Метрики для ASCII символов
    glm::vec3 m_CamRight{1,0,0};
    glm::vec3 m_CamUp{0,1,0};
    
    glm::vec2 m_IconUVMin;
    glm::vec2 m_IconUVMax;
};
