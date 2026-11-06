//
//  SelectTool.h
//  Cucumber
//
//  Выбор солидов и граней по клику.
//
//  Пока выбран солид, с его гранями работаем ТОЛЬКО по нему: луч не видит остальной уровень, поэтому грани, спрятанные
//  за другими солидами, достижимы (Alt выбирает грань, Shift тянет её). Alt промахнулся мимо выбранных солидов: берётся
//  грань под курсором из всего уровня.
//
//  Shift при выбранном солиде включает push/pull граней по схеме ExtrudeTool из TrenchBroom (см. PickExtrudeFace):
//  подсвечивается грань под курсором, а если луч в солид не попал, то тыльная грань ближайшего горизонтального ребра.
//  ЛКМ с протяжкой выдвигает её вдоль нормали с привязкой к сетке. Один commit на отпускании.
//
//  Перемещение выбранных солидов целиком (без режима вершин):
//    - протяжка ЛКМ по солиду (невыбранный выбирается и сразу тянется; Ctrl+клик только переключает выбор);
//    - клавиша G берёт выбранные солиды в движение за курсором, клик или Enter подтверждает;
//    - Shift+D копирует выбранные солиды и сразу двигает копии (Esc отменяет только движение, копии остаются).
//  Без ограничений солиды ходят в плоскости, через точку захвата: в ортографике это плоскость вида, в перспективе
//  горизонталь (или вертикальная плоскость лицом к камере, если смотрим почти горизонтально). X / Y / Z фиксируют
//  ось, Shift+X / Y / Z фиксируют плоскость, исключая ось (Blender). Привязка к сетке идёт по углу габарита.
//

#pragma once

#include "LevelPicking.h"
#include "LevelTool.h"
#include "geometry/Plane.h"

class SelectTool : public LevelTool
{
public:
    const char* name() const override { return "Select"; }

    void onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer) override;
    void cancel() override { m_drag = {}; m_move = {}; m_hasCandidate = false; m_altValid = false; }
    bool onKey(LevelEditSession& session, LevelKey key, bool shift) override;
    bool isBusy() const override { return m_drag.active || m_move.active; }
    bool showsHover() const override { return !m_pushPullMode && !m_altMode && !m_drag.active && !m_move.active; }

    std::string hint() const override;
    void collectPreview(const LevelEditSession& session, ToolPreview& out) const override;

    // Для тестов и UI
    bool hasCandidate() const { return m_hasCandidate; }
    size_t candidateFace() const { return m_candidateFace; }
    uint64_t candidateSolid() const { return m_candidateSolid; }
    ExtrudePick::Kind candidateKind() const { return m_candidatePick.kind; }
    bool hasAltFace() const { return m_altValid; }
    uint64_t altSolid() const { return m_altSolid; }
    uint64_t altFace() const { return m_altFaceUid; }
    double dragDelta() const { return m_drag.delta; }
    bool isMoving() const { return m_move.active; }
    glm::dvec3 moveDelta() const { return m_move.delta; }
    int moveConstraintAxis() const { return m_move.constraint.axis(); }
    bool moveConstraintIsPlane() const { return m_move.constraint.isPlane(); }

private:
    struct Drag
    {
        bool active = false;
        uint32_t view = 0;
        uint64_t solidUid = 0;
        size_t face = 0;
        Solid source;
        Solid preview;                      // последняя валидная форма
        std::vector<Geo::Plane> planes;
        glm::dvec3 normal{0.0};             // нормаль тащимой грани (наружу)

        // Откуда берётся смещение: по прямой через referencePoint вдоль normal (луч попал в грань) или по опорной
        // плоскости (грань выбрана по горизонтальному ребру): курсор в плоскости, смещение это проекция на normal
        bool usePlane = false;
        glm::dvec3 referencePoint{0.0};
        glm::dvec3 referenceNormal{0.0};
        double startParam = 0.0;
        double baseDistance = 0.0;
        double delta = 0.0;                 // смещение плоскости вдоль нормали (последнее валидное)
    };

    // Перемещение выбранных солидов целиком
    struct Move
    {
        bool active = false;
        bool fromDrag = false;           // начато протяжкой ЛКМ (подтверждается отпусканием), иначе клавишей G
        bool moved = false;              // курсор ушёл дальше порога протяжки
        uint32_t view = 0;
        glm::vec2 pressPx{0.0f};
        Edit::TranslateConstraint constraint;
        glm::dvec3 grabPoint{0.0};
        glm::dvec3 delta{0.0};
        uint64_t narrowUid = 0;          // клик по выбранному солиду из группы без протяжки сужает выбор до него
        std::string tag;                 // тег слияния undo для "дублировать и сдвинуть"
    };

    bool beginMove(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer, const glm::dvec3& grabPoint, bool fromDrag);
    void updateMove(LevelEditSession& session, const ViewportPointer& pointer);
    void finishMove(LevelEditSession& session);

    void updateCandidate(const LevelEditSession& session, const ViewportPointer& pointer);
    void updateAltFace(const LevelEditSession& session, const ViewportPointer& pointer, const std::vector<uint64_t>& context);
    static bool DragParam(const Drag& drag, const Edit::Ray& ray, double& out);
    bool beginDrag(const LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer);
    void updateDrag(LevelEditSession& session, const ViewportPointer& pointer);
    void finishDrag(LevelEditSession& session);

    Move m_move;
    uint64_t m_moveSerial = 0;

    // Последний указатель над видом: нужен клавишам G и Shift+D, у которых своего указателя нет
    bool m_hasLast = false;
    uint32_t m_lastView = 0;
    ViewportPointer m_last;

    bool m_pushPullMode = false;
    bool m_hasCandidate = false;
    uint32_t m_candidateView = 0;
    uint64_t m_candidateSolid = 0;
    size_t m_candidateFace = 0;
    ExtrudePick m_candidatePick;

    // Alt: грань выбранного солида под курсором (в том числе за другими солидами)
    bool m_altMode = false;
    bool m_altValid = false;
    uint64_t m_altSolid = 0;
    uint64_t m_altFaceUid = 0;

    Drag m_drag;
};
