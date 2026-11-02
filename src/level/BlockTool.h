//
//  BlockTool.h
//  Cucumber
//
//  Создание осевого бокса в три стадии:
//    Idle   - курсор показывает точку привязки; клик фиксирует первый угол основания
//    Base   - основание тянется по плоскости; клик (или отпускание после протяжки) фиксирует второй угол
//    Height - высота по нормали плоскости; клик создаёт бокс (один commit)
//  В ортографических видах высоту курсором не задать (она вдоль взгляда), поэтому бокс создаётся после основания,
//  а глубина берётся из последнего созданного бокса.
//

#pragma once

#include "LevelTool.h"

class BlockTool : public LevelTool
{
public:
    enum class Stage { Idle, Base, Height };

    const char* name() const override { return "Block"; }

    void onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer) override;
    void cancel() override;
    bool isBusy() const override { return m_stage != Stage::Idle; }

    std::string hint() const override;
    void collectPreview(const LevelEditSession& session, ToolPreview& out) const override;

    Stage stage() const { return m_stage; }

private:
    void updateIdle(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer);
    void updateBase(LevelEditSession& session, const ViewportPointer& pointer);
    void updateHeight(LevelEditSession& session, const ViewportPointer& pointer);

    bool choosePlane(const LevelEditSession& session, const ViewportPointer& pointer, const Edit::Ray& ray,
                     glm::dvec3& outPoint, glm::dvec3& outNormal) const;
    bool baseIsValid(double grid) const;
    void boxExtents(double height, glm::dvec3& mins, glm::dvec3& maxs) const;
    bool commitBox(LevelEditSession& session, double height);

    Stage m_stage = Stage::Idle;
    uint32_t m_view = 0;
    bool m_ortho = false;

    // Плоскость основания: нормаль направлена к наблюдателю (вдоль неё растёт высота)
    glm::dvec3 m_normal{0.0, 1.0, 0.0};
    int m_axis = 1;
    glm::dvec3 m_anchor{0.0};      // первый угол
    glm::dvec3 m_corner{0.0};      // второй угол (в той же плоскости)
    double m_height = 0.0;         // со знаком, вдоль m_normal

    glm::vec2 m_pressPx{0.0f};
    bool m_dragging = false;

    // Кандидат первого угла (Idle)
    bool m_hasCandidate = false;
    uint32_t m_candidateView = 0;
    glm::dvec3 m_candidate{0.0};

    // Что запомнили с прошлого бокса: ортографика берёт глубину и положение плоскости отсюда
    double m_lastExtent[3] = { 64.0, 64.0, 64.0 };
    double m_lastPlaneCoord[3] = { 0.0, 0.0, 0.0 };
};
