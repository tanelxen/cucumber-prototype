//
//  OverlayRender.h
//  Cucumber
//
//  Оверлей редактора на GPU: утолщённые сглаженные линии и маркеры постоянного экранного размера.
//  Рисуется в цель вида (с доступом к глубине сцены), не зависит от ImGui и от GizmoRender.
//
//  Линия это инстансированный квад, который вершинный шейдер расширяет в экранном пространстве до нужной толщины;
//  сглаживание считается во фрагментном шейдере по площади покрытия, поэтому толщина может быть любой (в том числе
//  дробной), а линия остаётся ровной на любых углах. Концы квадратные (стыки рёбер у углов без щелей). Линии
//  обрезаются по ближней плоскости в шейдере: отрезок, уходящий за камеру, рисуется до плоскости.
//
//  Толщина задаётся в ЛОГИЧЕСКИХ пикселях: на Retina линия выглядит так же. Масштаб считается по GL_VIEWPORT и
//  логическому размеру вида.
//
//  Использование (как у GizmoRender):
//    раз в кадр до рендера видов: Clear() в конце предыдущего, затем DrawLine / DrawPoint (мировые координаты);
//    для каждого вида при привязанной цели: Flush(view, proj, logicalSize);
//    в конце кадра: Clear().
//  Накопленное одно на все виды: каждый вид проецирует его своей камерой.
//

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

class OverlayRender
{
public:
    static OverlayRender& Get();

    enum class Depth
    {
        Tested,    // с тестом глубины сцены: скрытое объектами не видно
        Always,    // поверх всего, без теста глубины
        XRay       // видимое яркое, скрытое приглушённое (альфа XRayAlpha): глубина читается, но ничто не пропадает
    };

    enum class Shape { Square, Diamond, Circle };

    // GL-ресурсы (шейдеры, буферы). После создания контекста, как GizmoRender::Initialize.
    void Initialize();
    void Shutdown();

    // Накопление, мировые координаты. widthPx / sizePx в логических пикселях.
    void DrawLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color, float widthPx = 2.0f, Depth depth = Depth::Always);
    void DrawPoint(const glm::vec3& position, const glm::vec4& color, float sizePx, Shape shape = Shape::Square, Depth depth = Depth::Always);

    // Рисует накопленное. Вызывать при привязанной цели вида (глубина сцены нужна для Tested и XRay).
    // logicalSize: ЛОГИЧЕСКИЙ размер вида (ViewDesc::size), по нему и GL_VIEWPORT считается масштаб retina.
    void Flush(const glm::mat4& view, const glm::mat4& proj, const glm::ivec2& logicalSize);

    // В конце кадра
    void Clear();

    // Альфа скрытых частей у Depth::XRay
    void SetXRayAlpha(float alpha) { m_xrayAlpha = alpha; }

    // Смещение глубины линий к камере в NDC: чтобы линии на поверхности не мерцали от z-fighting.
    // Если у проекта обратная глубина (reversed-Z), знак надо поменять.
    void SetDepthBias(float bias) { m_depthBias = bias; }

    size_t lineCount() const;
    size_t pointCount() const;
    
public:
    // --- Постоянные наборы линий ---
    // Для статичных данных (каркас уровня): линии заливаются в GPU один раз при изменении, а не накапливаются
    // каждый кадр. Рисуются без теста глубины, одной толщины на весь набор. Накопленное через DrawLine / DrawPoint
    // к ним отношения не имеет и рисуется своим Flush.
    struct StaticLine { glm::vec3 a; glm::vec3 b; glm::vec4 color; };
    using LineSetId = uint32_t;     // 0 = нет набора
    
    LineSetId CreateLineSet();
    void UpdateLineSet(LineSetId id, const std::vector<StaticLine>& lines, float widthPx = 1.0f);
    void DestroyLineSet(LineSetId id);
    void DrawLineSet(LineSetId id, const glm::mat4& view, const glm::mat4& proj, const glm::ivec2& logicalSize);

private:
    struct LineInstance
    {
        glm::vec3 a;
        float width;
        glm::vec3 b;
        uint32_t color;      // RGBA8
    };
    static_assert(sizeof(LineInstance) == 32, "LineInstance layout is relied upon by the vertex shader");

    struct PointInstance
    {
        glm::vec3 position;
        float size;
        uint32_t color;
        float shape;
    };
    static_assert(sizeof(PointInstance) == 24, "PointInstance layout is relied upon by the vertex shader");

    // Слои рисуются в таком порядке: скрытая часть XRay (приглушённо), с тестом глубины, поверх всего
    enum Layer { XRayHidden = 0, TestedLayer = 1, AlwaysLayer = 2, LayerCount = 3 };

    struct Batch
    {
        std::vector<LineInstance> lines;
        std::vector<PointInstance> points;
    };

    struct Program
    {
        unsigned int id = 0;
        int viewProj = -1, viewport = -1, scale = -1, depthBias = -1, alphaScale = -1;
    };

    Program buildProgram(const char* vertex, const char* fragment) const;
    void upload();

    Batch m_batches[LayerCount];

    Program m_lineProgram;
    Program m_pointProgram;
    unsigned int m_vao = 0;
    unsigned int m_lineBuffer = 0;
    unsigned int m_pointBuffer = 0;
    size_t m_lineCapacity = 0;      // байт
    size_t m_pointCapacity = 0;
    bool m_uploaded = false;        // накопленное уже в GPU (виды переиспользуют одну заливку)
    size_t m_lineOffsets[LayerCount + 1] = {};
    size_t m_pointOffsets[LayerCount + 1] = {};

    float m_xrayAlpha = 0.35f;
    float m_depthBias = 2e-4f;
    bool m_initialized = false;
    
private:
    struct SavedState
    {
        bool depthTest = false, blend = false, cull = false, depthMask = true;
        int program = 0, vao = 0;
    };
    
    struct LineSet
    {
        unsigned int buffer = 0;
        size_t count = 0;
    };
    
    bool beginDraw(const glm::ivec2& logicalSize, glm::vec2& viewport, float& scale, SavedState& saved);
    void endDraw(const SavedState& saved);
    void setUniforms(const Program& program, bool depthTest, float alphaScale, const glm::mat4& viewProj,
                     const glm::vec2& viewport, float scale);
    void drawLineInstances(unsigned int buffer, size_t first, size_t count);
    void drawPointInstances(size_t first, size_t count);
    void drawLayer(Layer layer, bool depthTest, float alphaScale, const glm::mat4& viewProj, const glm::vec2& viewport, float scale);
    
    std::unordered_map<LineSetId, LineSet> m_lineSets;
    LineSetId m_nextLineSet = 1;
};
