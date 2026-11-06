//
//  LevelTool.h
//  Cucumber
//
//  Инструмент правки геометрии. Получает указатель каждого вида каждый кадр и сам ведёт своё состояние
//  (мини state-machine). Результат правки уходит через LevelEditSession::commit; пока правка не зафиксирована,
//  инструмент показывает превью (линии и подписи в мировых координатах), документ при этом не меняется.
//

#pragma once

#include "LevelDocument.h"
#include "ViewportPointer.h"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class LevelEditSession;

// Клавиши, которые панель вьюпорта передаёт активному инструменту (с учётом фокуса и состояния камеры)
enum class LevelKey
{
    X, Y, Z,        // ось (Shift: плоскость, исключающая ось)
    G,              // начать перемещение выделенных элементов
    Enter,          // подтвердить перемещение
    Num1, Num2,     // режим элементов: вершины / рёбра
    SelectAll,
    Copy, Cut, Paste,
    Duplicate,      // копия выбранных солидов рядом (смещение на шаг сетки)
    DuplicateMove   // копия выбранных солидов и сразу перемещение (Shift+D в Blender)
};

// Превью без привязки к рендеру: линии рисует GizmoRender, подписи панель вьюпорта (экранный текст ImGui)
struct ToolPreview
{
    struct Line { glm::vec3 a, b; glm::vec4 color; };
    struct Text { glm::vec3 position; std::string text; glm::vec4 color; };
    // Маркер постоянного экранного размера. Форма различает типы ручек: квадрат это вершина, ромб это ребро
    enum class Shape { Square, Diamond };
    struct Point { glm::vec3 position; glm::vec4 color; float sizePx; Shape shape = Shape::Square; };

    std::vector<Line> lines;
    std::vector<Text> texts;
    std::vector<Point> points;

    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    void addBox(const glm::dvec3& mins, const glm::dvec3& maxs, const glm::vec4& color);
    void addSolidEdges(const Solid& solid, const glm::vec4& color);
    void addFaceOutline(const Solid& solid, const Face& face, const glm::vec4& color, float lift = 0.1f);
    void addText(const glm::vec3& position, std::string text, const glm::vec4& color);
    void addPoint(const glm::vec3& position, const glm::vec4& color, float sizePx, Shape shape = Shape::Square);
};

// Число для подписи: целые без дробной части, остальные с одним знаком
std::string FormatUnits(double value);

class LevelTool
{
public:
    virtual ~LevelTool() = default;

    virtual const char* name() const = 0;

    // Вызывается для каждого вида каждый кадр. Инструмент в середине операции принимает ввод только от своего вида.
    virtual void onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer) = 0;

    // Клавиша от панели вьюпорта. true, если инструмент её обработал.
    virtual bool onKey(LevelEditSession& session, LevelKey key, bool shift) { (void)session; (void)key; (void)shift; return false; }

    // Прервать текущую операцию без изменения документа
    virtual void cancel() {}
    virtual bool isBusy() const { return false; }

    // Показывать ли подсветку солида под курсором (Select в режиме push/pull её скрывает)
    virtual bool showsHover() const { return false; }

    virtual std::string hint() const { return {}; }
    virtual void collectPreview(const LevelEditSession& session, ToolPreview& out) const { (void)session; (void)out; }
};
