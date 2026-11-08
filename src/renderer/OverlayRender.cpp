//
//  OverlayRender.cpp
//  Cucumber
//

#include "OverlayRender.h"

#include "GLState.h"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>
#include <iostream>

namespace
{
    // ------------------------------------------------------------------ линии
    const char* kLineVertex = R"(#version 330 core
layout(location = 0) in vec3 aA;
layout(location = 1) in float aWidth;
layout(location = 2) in vec3 aB;
layout(location = 3) in vec4 aColor;

uniform mat4 uViewProj;
uniform vec2 uViewport;     // физические пиксели
uniform float uScale;       // физических пикселей на логический
uniform float uDepthBias;

out vec4 vColor;
noperspective out float vAlong;     // экранное расстояние от центра отрезка вдоль него
noperspective out float vSide;      // экранное расстояние от оси поперёк
flat out float vHalfWidth;
flat out float vHalfLength;

void main()
{
    vec4 ca = uViewProj * vec4(aA, 1.0);
    vec4 cb = uViewProj * vec4(aB, 1.0);

    // Обрезка по ближней плоскости (z >= -w): иначе точка за камерой даёт отражённую проекцию
    float da = ca.z + ca.w;
    float db = cb.z + cb.w;
    if (da < 0.0 && db < 0.0)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);   // целиком за камерой: вне объёма отсечения
        vColor = vec4(0.0); vAlong = 0.0; vSide = 0.0; vHalfWidth = 0.0; vHalfLength = 0.0;
        return;
    }
    if (da < 0.0)      ca = mix(ca, cb, da / (da - db));
    else if (db < 0.0) cb = mix(ca, cb, da / (da - db));

    vec2 pa = (ca.xy / ca.w * 0.5 + 0.5) * uViewport;
    vec2 pb = (cb.xy / cb.w * 0.5 + 0.5) * uViewport;

    vec2 dir = pb - pa;
    float len = length(dir);
    dir = len > 1e-4 ? dir / len : vec2(1.0, 0.0);
    vec2 nrm = vec2(-dir.y, dir.x);

    float halfWidth = 0.5 * aWidth * uScale;
    float halfLength = len * 0.5 + halfWidth;      // квадратные концы: отрезок продлён на половину толщины
    float marginSide = halfWidth + 1.0;            // запас под сглаживание
    float marginAlong = halfLength + 1.0;

    bool endB = (gl_VertexID & 1) == 1;
    float side = (gl_VertexID & 2) == 2 ? 1.0 : -1.0;
    float along = endB ? 1.0 : -1.0;

    vec4 c = endB ? cb : ca;
    vec2 p = (pa + pb) * 0.5 + dir * along * marginAlong + nrm * side * marginSide;

    vColor = aColor;
    vAlong = along * marginAlong;
    vSide = side * marginSide;
    vHalfWidth = halfWidth;
    vHalfLength = halfLength;

    // xy умножаем на w конца: интерполяция остаётся корректной в перспективе
    gl_Position = vec4((p / uViewport * 2.0 - 1.0) * c.w, c.z - uDepthBias * c.w, c.w);
}
)";

    const char* kLineFragment = R"(#version 330 core
in vec4 vColor;
noperspective in float vAlong;
noperspective in float vSide;
flat in float vHalfWidth;
flat in float vHalfLength;

uniform float uAlphaScale;
out vec4 fragColor;

void main()
{
    // Покрытие пикселя: по ширине и по длине (концы тоже сглажены)
    float across = clamp(vHalfWidth + 0.5 - abs(vSide), 0.0, 1.0);
    float lengthwise = clamp(vHalfLength + 0.5 - abs(vAlong), 0.0, 1.0);
    float alpha = across * lengthwise * vColor.a * uAlphaScale;
    if (alpha <= 0.003) discard;
    fragColor = vec4(vColor.rgb, alpha);
}
)";

    // ------------------------------------------------------------------ маркеры
    const char* kPointVertex = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in float aSize;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aShape;

uniform mat4 uViewProj;
uniform vec2 uViewport;
uniform float uScale;
uniform float uDepthBias;

out vec4 vColor;
noperspective out vec2 vLocal;
flat out float vHalf;
flat out float vShape;
flat out float vOutline;

void main()
{
    vec4 c = uViewProj * vec4(aPosition, 1.0);
    if (c.z + c.w < 0.0)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vColor = vec4(0.0); vLocal = vec2(0.0); vHalf = 0.0; vShape = 0.0; vOutline = 0.0;
        return;
    }

    float halfSize = 0.5 * aSize * uScale;
    float extent = halfSize * 1.4 + 1.5;           // ромб выступает дальше квадрата: запас на обводку и сглаживание
    vec2 corner = vec2((gl_VertexID & 1) == 1 ? 1.0 : -1.0, (gl_VertexID & 2) == 2 ? 1.0 : -1.0);

    // Центр маркера прижимается к сетке пикселей: у чётного размера края попадают на границы пикселей, у нечётного
    // на их центры. Иначе края квадрата размываются.
    vec2 center = (c.xy / c.w * 0.5 + 0.5) * uViewport;
    bool evenSize = mod(floor(aSize * uScale + 0.5), 2.0) < 0.5;
    center = evenSize ? floor(center + 0.5) : floor(center) + 0.5;
    vec2 p = center + corner * extent;

    vColor = aColor;
    vLocal = corner * extent;
    vHalf = halfSize;
    vShape = aShape;
    vOutline = max(1.0, uScale);
    gl_Position = vec4((p / uViewport * 2.0 - 1.0) * c.w, c.z - uDepthBias * c.w, c.w);
}
)";

    const char* kPointFragment = R"(#version 330 core
in vec4 vColor;
noperspective in vec2 vLocal;
flat in float vHalf;
flat in float vShape;
flat in float vOutline;

uniform float uAlphaScale;
out vec4 fragColor;

void main()
{
    // Знаковое расстояние до края фигуры в пикселях (внутри отрицательное)
    float sd;
    vec2 a = abs(vLocal);
    if (vShape < 0.5)       sd = max(a.x, a.y) - vHalf;                       // квадрат
    else if (vShape < 1.5)  sd = (a.x + a.y - vHalf * 1.35) * 0.70710678;     // ромб (полудиагональ 1.35 * половины стороны)
    else                    sd = length(vLocal) - vHalf;                      // круг

    float outer = clamp(0.5 - sd, 0.0, 1.0);                                  // силуэт со сглаженным краем
    float inner = clamp(0.5 - (sd + vOutline), 0.0, 1.0);                     // заливка внутри тёмной обводки

    vec3 rgb = mix(vec3(0.0), vColor.rgb, inner);
    float alpha = outer * mix(0.8, vColor.a, inner) * uAlphaScale;
    if (alpha <= 0.003) discard;
    fragColor = vec4(rgb, alpha);
}
)";

    uint32_t PackColor(const glm::vec4& c)
    {
        const auto byte = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
        return byte(c.x) | (byte(c.y) << 8) | (byte(c.z) << 16) | (byte(c.w) << 24);
    }

    unsigned int Compile(unsigned int type, const char* source)
    {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "[OverlayRender] Shader compile error: " << log << std::endl;
        }
        return shader;
    }
}

OverlayRender& OverlayRender::Get()
{
    static OverlayRender instance;
    return instance;
}

OverlayRender::Program OverlayRender::buildProgram(const char* vertex, const char* fragment) const
{
    Program program;
    program.id = glCreateProgram();

    const GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
    glAttachShader(program.id, vs);
    glAttachShader(program.id, fs);
    glLinkProgram(program.id);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program.id, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(program.id, sizeof(log), nullptr, log);
        std::cerr << "[OverlayRender] Program link error: " << log << std::endl;
    }

    program.viewProj = glGetUniformLocation(program.id, "uViewProj");
    program.viewport = glGetUniformLocation(program.id, "uViewport");
    program.scale = glGetUniformLocation(program.id, "uScale");
    program.depthBias = glGetUniformLocation(program.id, "uDepthBias");
    program.alphaScale = glGetUniformLocation(program.id, "uAlphaScale");
    return program;
}

void OverlayRender::Initialize()
{
    if (m_initialized) return;

    m_lineProgram = buildProgram(kLineVertex, kLineFragment);
    m_pointProgram = buildProgram(kPointVertex, kPointFragment);

    // Атрибутов у квада нет: углы берутся из gl_VertexID. VAO нужен core profile, а указатели на инстансы
    // выставляются перед каждой отрисовкой (baseInstance в GL 4.1 нет)
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_lineBuffer);
    glGenBuffers(1, &m_pointBuffer);

    m_initialized = true;
    GLState::invalidate();
}

void OverlayRender::Shutdown()
{
    if (!m_initialized) return;

    glDeleteProgram(m_lineProgram.id);
    glDeleteProgram(m_pointProgram.id);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_lineBuffer);
    glDeleteBuffers(1, &m_pointBuffer);

    m_lineProgram = {};
    m_pointProgram = {};
    m_vao = m_lineBuffer = m_pointBuffer = 0;
    m_lineCapacity = m_pointCapacity = 0;
    m_initialized = false;
    
    for (auto& entry : m_lineSets)
    {
        glDeleteBuffers(1, &entry.second.buffer);
    }
    m_lineSets.clear();
    
    GLState::invalidate();
}

void OverlayRender::DrawLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color, float widthPx, Depth depth)
{
    const LineInstance instance{ a, widthPx, b, PackColor(color) };

    switch (depth)
    {
        case Depth::Tested: m_batches[TestedLayer].lines.push_back(instance); break;
        case Depth::Always: m_batches[AlwaysLayer].lines.push_back(instance); break;
        case Depth::XRay:   // скрытая часть приглушённо под яркой видимой
            m_batches[XRayHidden].lines.push_back(instance);
            m_batches[TestedLayer].lines.push_back(instance);
            break;
    }
    m_uploaded = false;
}

void OverlayRender::DrawPoint(const glm::vec3& position, const glm::vec4& color, float sizePx, Shape shape, Depth depth)
{
    const PointInstance instance{ position, sizePx, PackColor(color), static_cast<float>(shape) };

    switch (depth)
    {
        case Depth::Tested: m_batches[TestedLayer].points.push_back(instance); break;
        case Depth::Always: m_batches[AlwaysLayer].points.push_back(instance); break;
        case Depth::XRay:
            m_batches[XRayHidden].points.push_back(instance);
            m_batches[TestedLayer].points.push_back(instance);
            break;
    }
    m_uploaded = false;
}

void OverlayRender::Clear()
{
    for (Batch& batch : m_batches)
    {
        batch.lines.clear();
        batch.points.clear();
    }
    m_uploaded = false;
}

size_t OverlayRender::lineCount() const
{
    size_t count = 0;
    for (const Batch& batch : m_batches) count += batch.lines.size();
    return count;
}

size_t OverlayRender::pointCount() const
{
    size_t count = 0;
    for (const Batch& batch : m_batches) count += batch.points.size();
    return count;
}

void OverlayRender::upload()
{
    // Слои подряд в одном буфере: [XRayHidden][Tested][Always]
    std::vector<LineInstance> lines;
    std::vector<PointInstance> points;
    for (int layer = 0; layer < LayerCount; ++layer)
    {
        m_lineOffsets[layer] = lines.size();
        m_pointOffsets[layer] = points.size();
        lines.insert(lines.end(), m_batches[layer].lines.begin(), m_batches[layer].lines.end());
        points.insert(points.end(), m_batches[layer].points.begin(), m_batches[layer].points.end());
    }
    m_lineOffsets[LayerCount] = lines.size();
    m_pointOffsets[LayerCount] = points.size();

    const auto fill = [](unsigned int buffer, size_t& capacity, const void* data, size_t bytes)
    {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        if (bytes > capacity)
        {
            capacity = std::max(bytes, capacity + capacity / 2);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, GL_STREAM_DRAW);
        }
        if (bytes > 0) glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), data);
    };

    fill(m_lineBuffer, m_lineCapacity, lines.data(), lines.size() * sizeof(LineInstance));
    fill(m_pointBuffer, m_pointCapacity, points.data(), points.size() * sizeof(PointInstance));
    m_uploaded = true;
}

OverlayRender::LineSetId OverlayRender::CreateLineSet()
{
    if (!m_initialized) return 0;
    
    LineSet set;
    glGenBuffers(1, &set.buffer);
    
    const LineSetId id = m_nextLineSet++;
    m_lineSets.emplace(id, set);
    return id;
}

void OverlayRender::UpdateLineSet(LineSetId id, const std::vector<StaticLine>& lines, float widthPx)
{
    const auto it = m_lineSets.find(id);
    if (it == m_lineSets.end()) return;
    
    std::vector<LineInstance> instances;
    instances.reserve(lines.size());
    for (const StaticLine& line : lines)
        instances.push_back({ line.a, widthPx, line.b, PackColor(line.color) });
    
    glBindBuffer(GL_ARRAY_BUFFER, it->second.buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size() * sizeof(LineInstance)),
                 instances.empty() ? nullptr : instances.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    it->second.count = instances.size();
    GLState::invalidate();
}

void OverlayRender::DestroyLineSet(LineSetId id)
{
    const auto it = m_lineSets.find(id);
    if (it == m_lineSets.end()) return;
    
    glDeleteBuffers(1, &it->second.buffer);
    m_lineSets.erase(it);
}

bool OverlayRender::beginDraw(const glm::ivec2& logicalSize, glm::vec2& viewport, float& scale, SavedState& saved)
{
    // Физический размер цели берём из GL: желаемая толщина в логических пикселях, масштаб retina получается сам
    GLint viewportRect[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewportRect);
    if (viewportRect[2] <= 0 || viewportRect[3] <= 0) return false;
    
    viewport = glm::vec2(static_cast<float>(viewportRect[2]), static_cast<float>(viewportRect[3]));
    scale = viewport.x / static_cast<float>(std::max(1, logicalSize.x));
    
    // Состояние, которое трогаем
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    saved.depthMask = depthMask == GL_TRUE;
    saved.depthTest = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    saved.blend = glIsEnabled(GL_BLEND) == GL_TRUE;
    saved.cull = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &saved.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &saved.vao);
    
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(m_vao);
    return true;
}

void OverlayRender::endDraw(const SavedState& saved)
{
    // Указатели на инстансы выставлены в нашем VAO: сбрасываем делители, чтобы не влиять на чужие атрибуты
    for (GLuint index = 0; index < 4; ++index)
    {
        glVertexAttribDivisor(index, 0);
        glDisableVertexAttribArray(index);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glBindVertexArray(static_cast<GLuint>(saved.vao));
    glUseProgram(static_cast<GLuint>(saved.program));
    glDepthMask(saved.depthMask ? GL_TRUE : GL_FALSE);
    if (saved.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (saved.blend)     glEnable(GL_BLEND);      else glDisable(GL_BLEND);
    if (saved.cull)      glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    
    GLState::invalidate();
}

void OverlayRender::setUniforms(const Program& program, bool depthTest, float alphaScale, const glm::mat4& viewProj,
                                const glm::vec2& viewport, float scale)
{
    glUseProgram(program.id);
    glUniformMatrix4fv(program.viewProj, 1, GL_FALSE, &viewProj[0][0]);
    glUniform2f(program.viewport, viewport.x, viewport.y);
    glUniform1f(program.scale, scale);
    glUniform1f(program.depthBias, depthTest ? m_depthBias : 0.0f);
    glUniform1f(program.alphaScale, alphaScale);
}

void OverlayRender::drawLineInstances(unsigned int buffer, size_t first, size_t count)
{
    if (count == 0) return;
    
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    
    const GLsizei stride = sizeof(LineInstance);
    const char* base = reinterpret_cast<const char*>(first * sizeof(LineInstance));
    const auto attribute = [&](GLuint index, GLint size, GLenum type, GLboolean normalized, size_t offset)
    {
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, size, type, normalized, stride, base + offset);
        glVertexAttribDivisor(index, 1);
    };
    attribute(0, 3, GL_FLOAT, GL_FALSE, offsetof(LineInstance, a));
    attribute(1, 1, GL_FLOAT, GL_FALSE, offsetof(LineInstance, width));
    attribute(2, 3, GL_FLOAT, GL_FALSE, offsetof(LineInstance, b));
    attribute(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(LineInstance, color));
    
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(count));
}

void OverlayRender::drawPointInstances(size_t first, size_t count)
{
    if (count == 0) return;
    
    glBindBuffer(GL_ARRAY_BUFFER, m_pointBuffer);
    
    const GLsizei stride = sizeof(PointInstance);
    const char* base = reinterpret_cast<const char*>(first * sizeof(PointInstance));
    const auto attribute = [&](GLuint index, GLint size, GLenum type, GLboolean normalized, size_t offset)
    {
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, size, type, normalized, stride, base + offset);
        glVertexAttribDivisor(index, 1);
    };
    attribute(0, 3, GL_FLOAT, GL_FALSE, offsetof(PointInstance, position));
    attribute(1, 1, GL_FLOAT, GL_FALSE, offsetof(PointInstance, size));
    attribute(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(PointInstance, color));
    attribute(3, 1, GL_FLOAT, GL_FALSE, offsetof(PointInstance, shape));
    
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(count));
}

void OverlayRender::drawLayer(Layer layer, bool depthTest, float alphaScale, const glm::mat4& viewProj,
                              const glm::vec2& viewport, float scale)
{
    if (depthTest) glEnable(GL_DEPTH_TEST);
    else           glDisable(GL_DEPTH_TEST);
    
    const size_t lineFirst = m_lineOffsets[layer];
    const size_t lineCount = m_lineOffsets[layer + 1] - lineFirst;
    if (lineCount > 0)
    {
        setUniforms(m_lineProgram, depthTest, alphaScale, viewProj, viewport, scale);
        drawLineInstances(m_lineBuffer, lineFirst, lineCount);
    }
    
    const size_t pointFirst = m_pointOffsets[layer];
    const size_t pointCount = m_pointOffsets[layer + 1] - pointFirst;
    if (pointCount > 0)
    {
        setUniforms(m_pointProgram, depthTest, alphaScale, viewProj, viewport, scale);
        drawPointInstances(pointFirst, pointCount);
    }
}

void OverlayRender::DrawLineSet(LineSetId id, const glm::mat4& view, const glm::mat4& proj, const glm::ivec2& logicalSize)
{
    if (!m_initialized) return;
    
    const auto it = m_lineSets.find(id);
    if (it == m_lineSets.end() || it->second.count == 0) return;
    
    glm::vec2 viewport;
    float scale = 1.0f;
    SavedState saved;
    if (!beginDraw(logicalSize, viewport, scale, saved)) return;
    
    glDisable(GL_DEPTH_TEST);
    setUniforms(m_lineProgram, false, 1.0f, proj * view, viewport, scale);
    drawLineInstances(it->second.buffer, 0, it->second.count);
    
    endDraw(saved);
}

void OverlayRender::Flush(const glm::mat4& view, const glm::mat4& proj, const glm::ivec2& logicalSize)
{
    if (!m_initialized || (lineCount() == 0 && pointCount() == 0)) return;
    if (!m_uploaded) upload();
    
    glm::vec2 viewport;
    float scale = 1.0f;
    SavedState saved;
    if (!beginDraw(logicalSize, viewport, scale, saved)) return;
    
    const glm::mat4 viewProj = proj * view;
    
    drawLayer(XRayHidden, false, m_xrayAlpha, viewProj, viewport, scale);   // скрытая часть XRay, приглушённо
    drawLayer(TestedLayer, true, 1.0f, viewProj, viewport, scale);          // видимое с тестом глубины
    drawLayer(AlwaysLayer, false, 1.0f, viewProj, viewport, scale);         // поверх всего
    
    endDraw(saved);
}
