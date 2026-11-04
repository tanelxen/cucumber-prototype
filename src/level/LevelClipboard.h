//
//  LevelClipboard.h
//  Cucumber
//
//  Буфер обмена солидов уровня. Общий на процесс (статический), поэтому копировать можно между уровнями и сценами.
//  Материалы хранятся именами: таблицы материалов у документов разные, индексы граней при вставке пересчитываются.
//

#pragma once

#include "Bounds.h"
#include "LevelDocument.h"

#include <string>
#include <vector>

class LevelClipboard
{
public:
    static LevelClipboard& Get();

    bool empty() const { return m_solids.empty(); }
    size_t count() const { return m_solids.size(); }

    // Запоминает копии солидов вместе с именами их материалов (прежнее содержимое заменяется)
    void assign(const LevelDocument& source, const std::vector<const Solid*>& solids);

    // Габариты скопированных солидов в исходных координатах
    AABB bounds() const;

    // Новые экземпляры для вставки в документ target: новые uid, материалы по именам (недостающие добавляются
    // в таблицу), сдвиг на delta (с компенсацией UV при textureLock)
    std::vector<Solid> instantiate(LevelDocument& target, const glm::vec3& delta, bool textureLock) const;

    void clear();

private:
    std::vector<std::string> m_materials;
    std::vector<Solid> m_solids;     // face.material индексирует m_materials
};
