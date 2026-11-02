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

    double Snap(double value, double grid);                       // grid <= 0: без привязки
    glm::dvec3 SnapVec(const glm::dvec3& v, double grid);

    int DominantAxis(const glm::dvec3& v);                        // 0, 1 или 2: ось с наибольшей |компонентой|
    glm::dvec3 AxisNormal(const glm::dvec3& v);                   // +-единичный вектор вдоль доминирующей оси
}
