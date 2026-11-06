//
//  VertexTool.h
//  Cucumber
//
//  Перемещение вершин и рёбер выделенных солидов в стиле Blender.
//
//  Режимы (различаются на экране): 1 вершины, квадратные ручки в вершинах; 2 рёбра, ромбовидные ручки в серединах рёбер.
//           В режиме рёбер вершины не рисуются, а ребро берётся только за свою ручку.
//  Выбор:   клик по ручке, Ctrl+клик переключает, A выбирает все вершины.
//           Вершины, совпадающие по положению у соседних выделенных солидов, выбираются вместе.
//  Движение: протяжка ЛКМ по элементу или клавиша G. Без ограничений элемент ходит в плоскости, параллельной
//           экрану (глубина не меняется). X / Y / Z фиксируют ось, Shift+X / Y / Z фиксируют плоскость, исключая ось,
//           повторное нажатие снимает ограничение. Подтверждение: отпускание (протяжка) или клик / Enter (G).
//           Esc отменяет. Движение привязано к сетке по положению якорной вершины.
//  Тело пересобирается как выпуклая оболочка новых положений вершин; недопустимые положения (вершина ушла внутрь
//  тела, объём схлопнулся) пропускаются: остаётся последняя допустимая форма.
//

#pragma once

#include "LevelTool.h"

#include <vector>

class VertexTool : public LevelTool
{
public:
    enum class Mode { Vertex, Edge };

    const char* name() const override { return "Vertex"; }

    void onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer) override;
    bool onKey(LevelEditSession& session, LevelKey key, bool shift) override;
    void cancel() override;
    bool isBusy() const override { return m_grab.active; }

    std::string hint() const override;
    void collectPreview(const LevelEditSession& session, ToolPreview& out) const override;

    // Для тестов и UI
    Mode mode() const { return m_mode; }
    size_t selectedCount() const { return m_selected.size(); }
    bool hasHover() const { return m_hover.valid; }
    int constraintAxis() const { return m_grab.constraint.axis(); }
    bool constraintIsPlane() const { return m_grab.constraint.isPlane(); }
    glm::dvec3 grabDelta() const { return m_grab.delta; }

private:
    // Выбранная вершина. Хранится по положению, а не по индексу: индексы меняются при пересборке тела
    // (push/pull, undo), а положение остаётся осмысленным.
    struct Element
    {
        uint64_t solidUid = 0;
        glm::vec3 position{0.0f};
    };

    struct Hover
    {
        bool valid = false;
        bool isEdge = false;
        uint64_t solidUid = 0;
        glm::vec3 a{0.0f};
        glm::vec3 b{0.0f};
    };

    struct SolidEdit
    {
        uint64_t uid = 0;
        Solid source;
        Solid preview;                     // последняя допустимая форма
        std::vector<uint32_t> moved;       // индексы вершин в source
    };

    struct Grab
    {
        bool active = false;
        bool fromDrag = false;             // начато протяжкой ЛКМ (подтверждается отпусканием), иначе клавишей G
        uint32_t view = 0;
        glm::vec2 pressPx{0.0f};
        Edit::TranslateConstraint constraint;
        glm::dvec3 anchor{0.0};            // вершина: через неё идёт плоскость движения и по ней считается привязка к сетке
        glm::dvec3 delta{0.0};
        std::vector<SolidEdit> edits;
    };

    Hover pickElement(const LevelEditSession& session, const ViewportPointer& pointer) const;
    void selectHover(const LevelEditSession& session, const Hover& hover, bool additive);
    void toggleHover(const LevelEditSession& session, const Hover& hover);
    bool hoverIsSelected(const Hover& hover) const;
    void addElementsAt(const LevelEditSession& session, const glm::vec3& position);
    void removeElementsAt(const glm::vec3& position);
    bool isSelectedAt(uint64_t solidUid, const glm::vec3& position) const;
    void selectAllVertices(const LevelEditSession& session);

    bool beginGrab(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer, bool fromDrag, const glm::vec3* anchorHint);
    void updateGrab(LevelEditSession& session, const ViewportPointer& pointer);
    bool applyDelta(const LevelEditSession& session, const glm::dvec3& delta);
    void finishGrab(LevelEditSession& session);

    Mode m_mode = Mode::Vertex;
    std::vector<Element> m_selected;

    Hover m_hover;
    uint32_t m_hoverView = 0;

    // Последний указатель над видом: нужен клавише G, у которой своего указателя нет
    bool m_hasLast = false;
    uint32_t m_lastView = 0;
    ViewportPointer m_last;

    Grab m_grab;
};
