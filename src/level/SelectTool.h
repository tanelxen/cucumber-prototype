//
//  SelectTool.h
//  Cucumber
//
//  Выбор солидов и граней по клику. При зажатом Shift и непустом выделении включается режим смещения граней
//  (push/pull, как в TrenchBroom): подсвечивается грань выделенного солида, подходящая курсору (в том числе
//  задняя), ЛКМ с протяжкой выдвигает её вдоль своей нормали с привязкой к сетке. Один commit на отпускании.
//

#pragma once

#include "LevelTool.h"
#include "geometry/Plane.h"

class SelectTool : public LevelTool
{
public:
    const char* name() const override { return "Select"; }

    void onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer) override;
    void cancel() override { m_drag = {}; m_hasCandidate = false; }
    bool isBusy() const override { return m_drag.active; }
    bool showsHover() const override { return !m_pushPullMode && !m_drag.active; }

    std::string hint() const override;
    void collectPreview(const LevelEditSession& session, ToolPreview& out) const override;

    // Для тестов и UI
    bool hasCandidate() const { return m_hasCandidate; }
    size_t candidateFace() const { return m_candidateFace; }
    uint64_t candidateSolid() const { return m_candidateSolid; }
    double dragDelta() const { return m_drag.delta; }

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
        glm::dvec3 linePoint{0.0};
        glm::dvec3 normal{0.0};
        double startParam = 0.0;
        double baseDistance = 0.0;
        double delta = 0.0;                 // смещение плоскости вдоль нормали (последнее валидное)
    };

    void updateCandidate(const LevelEditSession& session, const ViewportPointer& pointer);
    bool beginDrag(const LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer);
    void updateDrag(LevelEditSession& session, const ViewportPointer& pointer);
    void finishDrag(LevelEditSession& session);

    bool m_pushPullMode = false;
    bool m_hasCandidate = false;
    uint32_t m_candidateView = 0;
    uint64_t m_candidateSolid = 0;
    size_t m_candidateFace = 0;

    Drag m_drag;
};
