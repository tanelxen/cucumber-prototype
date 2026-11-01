//
//  LevelHistory.h
//  Cucumber
//
//  Правки документа и undo/redo. Любая правка это список SolidChange: солид создан (before пуст), удалён
//  (after пуст) или заменён (оба заполнены). Одного такого примитива хватает для перемещения, push/pull граней,
//  правки вершин, clip, смены материала и UV: тела маленькие, поэтому хранится снимок, а не обратная операция.
//
//  Правка применяется атомарно: если хоть одно изменение не проходит проверку, документ не меняется.
//

#pragma once

#include "LevelDocument.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct SolidChange
{
    uint64_t uid = 0;
    std::optional<Solid> before;   // nullopt: солид создан этой правкой
    std::optional<Solid> after;    // nullopt: солид удалён этой правкой
    size_t index = 0;              // позиция в LevelDocument::solids (заполняет история; нужна для восстановления порядка)

    static SolidChange Created(Solid solid);
    static SolidChange Removed(Solid solid);
    static SolidChange Modified(Solid before, Solid after);
};

// Что изменилось в документе (для потребителей: меш, физика, UI)
struct ChangeSet
{
    std::vector<uint64_t> added;
    std::vector<uint64_t> modified;
    std::vector<uint64_t> removed;

    bool empty() const { return added.empty() && modified.empty() && removed.empty(); }
    void merge(const ChangeSet& other);
};

class LevelHistory
{
public:
    // Применяет правку к документу и кладёт её в стек. Ветка redo отбрасывается.
    // false (документ не тронут), если какое-то изменение некорректно: error объясняет почему.
    bool commit(LevelDocument& document, std::string name, std::vector<SolidChange> changes,
                ChangeSet& out, std::string& error);

    bool undo(LevelDocument& document, ChangeSet& out);
    bool redo(LevelDocument& document, ChangeSet& out);

    bool canUndo() const { return m_cursor > 0; }
    bool canRedo() const { return m_cursor < m_edits.size(); }
    const std::string& undoName() const;   // пусто, если откатывать нечего
    const std::string& redoName() const;

    // Текущее состояние считается сохранённым (после записи на диск)
    void markSaved() { m_saved = m_cursor; }
    bool isDirty() const { return m_saved != m_cursor; }

    void clear();

private:
    struct Edit
    {
        std::string name;
        std::vector<SolidChange> changes;
    };

    static constexpr size_t kNoMark = static_cast<size_t>(-1);
    static constexpr size_t kMaxEdits = 512;

    std::vector<Edit> m_edits;
    size_t m_cursor = 0;          // число применённых правок
    size_t m_saved = 0;           // значение m_cursor на момент сохранения; kNoMark: сохранённое состояние недостижимо
};
