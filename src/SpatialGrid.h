//
//  SpatialGrid.h
//  Cucumber
//
//  Хэш-сетка поверх плотных id [0..N). Элемент регистрируется во всех ячейках, которые
//  перекрывает его AABB. Один и тот же класс обслуживает:
//    - отсечение по фрустуму камеры,
//    - запросы по свету (сфера/конус/ortho-коробка -> AABB),
//    - точечные и малые запросы.
//
//  Не потокобезопасен для insert; query потокобезопасен, если у каждого потока свой QueryScratch.
//

#pragma once

#include "Bounds.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

class SpatialGrid
{
public:
    // Для дедупликации: элемент, лежащий в нескольких ячейках, возвращается один раз.
    struct QueryScratch
    {
        std::vector<uint32_t> stamp;
        uint32_t counter = 0;
    };

    explicit SpatialGrid(float cellSize = 1024.0f) : m_cellSize(cellSize) {}

    void clear()
    {
        m_lookup.clear();
        m_cells.clear();
        m_itemBounds.clear();
    }

    // id должны быть плотными (индексы в массиве поверхностей/объектов).
    void insert(uint32_t id, const AABB& bounds)
    {
        if (id >= m_itemBounds.size()) m_itemBounds.resize(id + 1);
        m_itemBounds[id] = bounds;

        const glm::ivec3 lo = cellOf(bounds.min);
        const glm::ivec3 hi = cellOf(bounds.max);
        for (int z = lo.z; z <= hi.z; ++z)
            for (int y = lo.y; y <= hi.y; ++y)
                for (int x = lo.x; x <= hi.x; ++x)
                    getOrCreate(x, y, z).items.push_back(id);
    }

    // Точные тесты по AABB элемента выполняются внутри, наружу попадают только пересекающиеся.
    void queryAABB(const AABB& box, QueryScratch& scratch, std::vector<uint32_t>& out) const
    {
        begin(scratch);

        const glm::ivec3 lo = cellOf(box.min);
        const glm::ivec3 hi = cellOf(box.max);
        const int64_t span = int64_t(hi.x - lo.x + 1) * (hi.y - lo.y + 1) * (hi.z - lo.z + 1);

        if (span > int64_t(m_cells.size()))
        {
            // Коробка огромна — дешевле пройтись по занятым ячейкам, чем по её объёму.
            for (const Cell& c : m_cells)
            {
                if (c.bounds.intersects(box)) collect(c, box, scratch, out);
            }
            return;
        }

        for (int z = lo.z; z <= hi.z; ++z)
            for (int y = lo.y; y <= hi.y; ++y)
                for (int x = lo.x; x <= hi.x; ++x)
                {
                    const auto it = m_lookup.find(key(x, y, z));
                    if (it != m_lookup.end()) collect(m_cells[it->second], box, scratch, out);
                }
    }

    // Для фрустума идём по занятым ячейкам: их единицы-тысячи, это дешевле, чем обход объёма
    // фрустума с хэш-поиском. Иерархия (BVH/октодерево) нужна только когда ячеек десятки тысяч.
    void queryFrustum(const Frustum& frustum, QueryScratch& scratch, std::vector<uint32_t>& out) const
    {
        begin(scratch);

        for (const Cell& c : m_cells)
        {
            if (!frustum.intersects(c.bounds)) continue;

            for (uint32_t id : c.items)
            {
                if (!visit(scratch, id)) continue;
                if (frustum.intersects(m_itemBounds[id])) out.push_back(id);
            }
        }
    }

    size_t cellCount() const { return m_cells.size(); }
    size_t itemCount() const { return m_itemBounds.size(); }

private:
    struct Cell
    {
        AABB bounds;                 // границы самой ячейки (не плотные)
        std::vector<uint32_t> items;
    };

    glm::ivec3 cellOf(const glm::vec3& p) const
    {
        return glm::ivec3(int(std::floor(p.x / m_cellSize)),
                          int(std::floor(p.y / m_cellSize)),
                          int(std::floor(p.z / m_cellSize)));
    }

    static uint64_t key(int x, int y, int z)
    {
        constexpr int64_t kOffset = 1 << 20;
        constexpr uint64_t kMask = 0x1FFFFF; // 21 бит на ось
        return (uint64_t(x + kOffset) & kMask) |
               ((uint64_t(y + kOffset) & kMask) << 21) |
               ((uint64_t(z + kOffset) & kMask) << 42);
    }

    Cell& getOrCreate(int x, int y, int z)
    {
        const auto [it, inserted] = m_lookup.try_emplace(key(x, y, z), uint32_t(m_cells.size()));
        if (inserted)
        {
            Cell cell;
            cell.bounds.min = glm::vec3(float(x), float(y), float(z)) * m_cellSize;
            cell.bounds.max = cell.bounds.min + glm::vec3(m_cellSize);
            m_cells.push_back(std::move(cell));
        }
        return m_cells[it->second];
    }

    void begin(QueryScratch& s) const
    {
        if (s.stamp.size() < m_itemBounds.size()) s.stamp.resize(m_itemBounds.size(), 0);
        if (++s.counter == 0)
        {
            std::fill(s.stamp.begin(), s.stamp.end(), 0u);
            s.counter = 1;
        }
    }

    static bool visit(QueryScratch& s, uint32_t id)
    {
        if (s.stamp[id] == s.counter) return false;
        s.stamp[id] = s.counter;
        return true;
    }

    void collect(const Cell& c, const AABB& box, QueryScratch& s, std::vector<uint32_t>& out) const
    {
        for (uint32_t id : c.items)
        {
            if (visit(s, id) && m_itemBounds[id].intersects(box)) out.push_back(id);
        }
    }

    float m_cellSize;
    std::unordered_map<uint64_t, uint32_t> m_lookup;
    std::vector<Cell> m_cells;
    std::vector<AABB> m_itemBounds;
};
