//
//  GeometryRevision.h
//  Cucumber
//
//  Единая нумерация ревизий статической геометрии карты (MapMesh и LevelEditMesh).
//  Значение уникально на всё время работы, поэтому ключ статики в RenderScene не совпадёт
//  при переключении между запечённой геометрией и геометрией режима правки.
//  0 зарезервирован: "геометрия ещё не собрана".
//

#pragma once

#include <atomic>
#include <cstdint>

inline uint64_t NextGeometryRevision()
{
    static std::atomic<uint64_t> counter{0};
    return ++counter;
}
