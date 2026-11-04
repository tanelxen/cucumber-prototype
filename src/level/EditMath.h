//
//  EditMath.h
//  Cucumber
//
//  Геометрия курсора для инструментов правки: пересечения луча с плоскостью и прямой, привязка к сетке.
//  Всё в double: координаты уровня бывают порядка тысяч, а привязка к сетке должна быть точной.
//

#pragma once

#include <glm/glm.hpp>

namespace Edit
{
    struct Ray
    {
        glm::dvec3 origin{0.0};
        glm::dvec3 dir{0.0, 0.0, -1.0};   // единичный
        double maxDistance = 8192.0;

        glm::dvec3 at(double t) const { return origin + dir * t; }
    };

    // Допуск попадания в пикселях, пересчитанный в мировые единицы: base + perDistance * расстояние_до_камеры.
    // В перспективе размер пикселя растёт с расстоянием (perDistance), в ортографии постоянен (base).
    struct PickTolerance
    {
        double base = 0.0;
        double perDistance = 0.0;
        double at(double distance) const { return base + perDistance * distance; }
    };

    // Точка луча на плоскости: t в [0, maxDistance]. false, если луч почти параллелен плоскости или она позади.
    bool RayPlane(const Ray& ray, const glm::dvec3& planePoint, const glm::dvec3& planeNormal, double& outT);

    // Параметр t точки прямой P(t) = linePoint + lineDir * t, ближайшей к лучу (lineDir единичный).
    // false, если луч идёт почти вдоль прямой (меньше ~6 градусов): параметр там неопределён и "скачет".
    bool ClosestLineParam(const Ray& ray, const glm::dvec3& linePoint, const glm::dvec3& lineDir, double& outT);

    struct SegmentApproach
    {
        double rayT = 0.0;        // расстояние вдоль луча до ближайшей точки
        double distance = 0.0;    // расстояние между ближайшими точками
        glm::dvec3 rayPoint{0.0};
        glm::dvec3 segmentPoint{0.0};
    };

    // Ближайшие точки между лучом и отрезком [a, b]
    SegmentApproach ClosestRaySegment(const Ray& ray, const glm::dvec3& a, const glm::dvec3& b);

    // Перемещение с ограничениями в стиле Blender, общее для инструментов вершин и солидов.
    //   Без ограничений: движение в плоскости freeNormal через planePoint (в ортографии это плоскость вида,
    //   в перспективе чаще горизонтальная или параллельная экрану); если freeNormal осевая, эта ось не двигается.
    //   Ось: движение вдоль оси X / Y / Z. Плоскость: движение в плоскости, исключающей ось.
    // Привязка к сетке идёт по положению snapReference после сдвига (а не по самому сдвигу): вершина или угол
    // габарита, не лежащие на сетке, после движения встают на неё.
    class TranslateConstraint
    {
    public:
        // planePoint: точка захвата (через неё проходят плоскость и прямая движения, поэтому курсор "держит" её);
        // snapReference: точка, чьё новое положение привязывается к сетке
        void begin(const Ray& startRay, const glm::dvec3& planePoint, const glm::dvec3& snapReference, const glm::dvec3& freeNormal);

        // Ограничение осью (plane = false) или плоскостью, исключающей ось (plane = true). Повторное нажатие снимает.
        void toggleAxis(int axis, bool plane);
        void clear() { m_axis = -1; m_plane = false; }
        int axis() const { return m_axis; }
        bool isPlane() const { return m_plane; }

        // Сдвиг для текущего луча. Ограничение считается от исходного луча, поэтому смена оси посреди движения не
        // "прыгает": результат это проекция всего пути мыши. false, если луч параллелен плоскости или прямой.
        bool compute(const Ray& ray, double grid, glm::dvec3& out) const;

        const glm::dvec3& planePoint() const { return m_planePoint; }

    private:
        Ray m_startRay;
        glm::dvec3 m_planePoint{0.0};
        glm::dvec3 m_snapReference{0.0};
        glm::dvec3 m_freeNormal{0.0, 1.0, 0.0};
        int m_axis = -1;
        bool m_plane = false;
    };

    double Snap(double value, double grid);                       // grid <= 0: без привязки
    glm::dvec3 SnapVec(const glm::dvec3& v, double grid);

    int DominantAxis(const glm::dvec3& v);                        // 0, 1 или 2: ось с наибольшей |компонентой|
    glm::dvec3 AxisNormal(const glm::dvec3& v);                   // +-единичный вектор вдоль доминирующей оси
}
