//
//  LevelTool.cpp
//  Cucumber
//

#include "LevelTool.h"

#include <cmath>
#include <cstdio>

void ToolPreview::addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color)
{
    lines.push_back({ a, b, color });
}

void ToolPreview::addBox(const glm::dvec3& mins, const glm::dvec3& maxs, const glm::vec4& color)
{
    glm::vec3 c[8];
    for (int i = 0; i < 8; ++i)
    {
        c[i] = { float((i & 1) ? maxs.x : mins.x), float((i & 2) ? maxs.y : mins.y), float((i & 4) ? maxs.z : mins.z) };
    }
    // Рёбра: пары вершин, отличающиеся одним битом
    for (int i = 0; i < 8; ++i)
    {
        for (int bit = 1; bit <= 4; bit <<= 1)
        {
            if (!(i & bit)) addLine(c[i], c[i | bit], color);
        }
    }
}

void ToolPreview::addSolidEdges(const Solid& solid, const glm::vec4& color)
{
    for (const Face& face : solid.faces)
    {
        for (size_t i = 0; i < face.loop.size(); ++i)
        {
            const uint32_t a = face.loop[i];
            const uint32_t b = face.loop[(i + 1) % face.loop.size()];
            if (a < b) addLine(solid.vertices[a], solid.vertices[b], color);   // каждое ребро делят две грани: рисуем одно направление
        }
    }
}

void ToolPreview::addFaceOutline(const Solid& solid, const Face& face, const glm::vec4& color, float lift)
{
    const glm::vec3 offset = FaceNormal(solid, face) * lift;
    for (size_t i = 0; i < face.loop.size(); ++i)
    {
        addLine(solid.vertices[face.loop[i]] + offset, solid.vertices[face.loop[(i + 1) % face.loop.size()]] + offset, color);
    }
}

void ToolPreview::addText(const glm::vec3& position, std::string text, const glm::vec4& color)
{
    texts.push_back({ position, std::move(text), color });
}

void ToolPreview::addPoint(const glm::vec3& position, const glm::vec4& color, float sizePx, Shape shape)
{
    points.push_back({ position, color, sizePx, shape });
}

std::string FormatUnits(double value)
{
    char buffer[32];
    if (std::abs(value - std::round(value)) < 0.01) std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
    else                                            std::snprintf(buffer, sizeof(buffer), "%.1f", value);
    return buffer;
}
