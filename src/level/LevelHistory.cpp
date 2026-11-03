//
//  LevelHistory.cpp
//  Cucumber
//

#include "LevelHistory.h"

#include <algorithm>
#include <unordered_set>

SolidChange SolidChange::Created(Solid solid)
{
    SolidChange change;
    change.uid = solid.uid;
    change.after = std::move(solid);
    return change;
}

SolidChange SolidChange::Removed(Solid solid)
{
    SolidChange change;
    change.uid = solid.uid;
    change.before = std::move(solid);
    return change;
}

SolidChange SolidChange::Modified(Solid before, Solid after)
{
    SolidChange change;
    change.uid = before.uid;
    change.before = std::move(before);
    change.after = std::move(after);
    return change;
}

namespace
{
    constexpr size_t kNotFound = static_cast<size_t>(-1);

    size_t FindSolid(const LevelDocument& document, uint64_t uid)
    {
        for (size_t i = 0; i < document.solids.size(); ++i)
        {
            if (document.solids[i].uid == uid) return i;
        }
        return kNotFound;
    }

    void PushUnique(std::vector<uint64_t>& list, uint64_t uid)
    {
        if (std::find(list.begin(), list.end(), uid) == list.end()) list.push_back(uid);
    }

    void ApplyForward(LevelDocument& document, SolidChange& change, ChangeSet& out)
    {
        if (change.before && change.after)
        {
            document.solids[FindSolid(document, change.uid)] = *change.after;
            PushUnique(out.modified, change.uid);
        }
        else if (change.after)
        {
            change.index = document.solids.size();
            document.solids.push_back(*change.after);
            PushUnique(out.added, change.uid);
        }
        else
        {
            const size_t index = FindSolid(document, change.uid);
            change.index = index;
            document.solids.erase(document.solids.begin() + static_cast<std::ptrdiff_t>(index));
            PushUnique(out.removed, change.uid);
        }
    }

    void ApplyBackward(LevelDocument& document, const SolidChange& change, ChangeSet& out)
    {
        if (change.before && change.after)
        {
            document.solids[FindSolid(document, change.uid)] = *change.before;
            PushUnique(out.modified, change.uid);
        }
        else if (change.after)
        {
            const size_t index = FindSolid(document, change.uid);
            document.solids.erase(document.solids.begin() + static_cast<std::ptrdiff_t>(index));
            PushUnique(out.removed, change.uid);
        }
        else
        {
            const size_t index = std::min(change.index, document.solids.size());
            document.solids.insert(document.solids.begin() + static_cast<std::ptrdiff_t>(index), *change.before);
            PushUnique(out.added, change.uid);
        }
    }

    bool Validate(const LevelDocument& document, const std::vector<SolidChange>& changes, std::string& error)
    {
        if (changes.empty()) { error = "Empty edit"; return false; }

        std::unordered_set<uint64_t> seen;
        for (const SolidChange& change : changes)
        {
            const std::string who = "solid " + UidToString(change.uid) + ": ";

            if (change.uid == 0 || !seen.insert(change.uid).second) { error = who + "missing or repeated uid"; return false; }
            if (!change.before && !change.after) { error = who + "change does nothing"; return false; }
            if (change.before && change.before->uid != change.uid) { error = who + "uid of 'before' differs"; return false; }

            const bool exists = FindSolid(document, change.uid) != kNotFound;
            if (change.before && !exists) { error = who + "does not exist in the document"; return false; }
            if (!change.before && exists) { error = who + "already exists in the document"; return false; }

            if (change.after)
            {
                if (change.after->uid != change.uid) { error = who + "uid of 'after' differs"; return false; }

                std::string reason;
                if (!ValidateSolid(*change.after, document.materials.size(), reason)) { error = who + reason; return false; }
            }
        }
        return true;
    }
}

void ChangeSet::merge(const ChangeSet& other)
{
    // Порядок: создан и тут же удалён = ничего; создан и изменён = создан; изменён и удалён = удалён
    for (uint64_t uid : other.added)
    {
        const auto removedBefore = std::find(removed.begin(), removed.end(), uid);
        if (removedBefore != removed.end())
        {
            removed.erase(removedBefore);   // удалён, потом возвращён (undo): для потребителей это изменение
            PushUnique(modified, uid);
        }
        else
        {
            PushUnique(added, uid);
        }
    }
    for (uint64_t uid : other.modified)
    {
        if (std::find(added.begin(), added.end(), uid) == added.end()) PushUnique(modified, uid);
    }
    for (uint64_t uid : other.removed)
    {
        const auto addedBefore = std::find(added.begin(), added.end(), uid);
        if (addedBefore != added.end())
        {
            added.erase(addedBefore);   // создан и удалён внутри одного накопления
        }
        else
        {
            modified.erase(std::remove(modified.begin(), modified.end(), uid), modified.end());
            PushUnique(removed, uid);
        }
    }
}

bool LevelHistory::tryCoalesce(const std::vector<SolidChange>& changes, const std::string& tag)
{
    // Сохранённое состояние не трогаем: слияние оставило бы курсор на месте и спрятало бы признак "изменён"
    if (tag.empty() || m_cursor == 0 || m_cursor != m_edits.size() || m_saved == m_cursor) return false;

    Edit& last = m_edits.back();
    if (last.tag != tag) return false;

    for (const SolidChange& change : changes)
    {
        if (!change.before || !change.after) return false;
        for (const SolidChange& previous : last.changes)
        {
            if (previous.uid == change.uid && (!previous.before || !previous.after)) return false;
        }
    }

    for (const SolidChange& change : changes)
    {
        const auto it = std::find_if(last.changes.begin(), last.changes.end(),
                                     [&](const SolidChange& previous) { return previous.uid == change.uid; });
        if (it != last.changes.end()) it->after = change.after;   // before остаётся самым первым: undo вернёт к исходному
        else                          last.changes.push_back(change);
    }
    return true;
}

void LevelHistory::sealLast()
{
    if (!m_edits.empty()) m_edits.back().tag.clear();
}

bool LevelHistory::commit(LevelDocument& document, std::string name, std::vector<SolidChange> changes,
                          ChangeSet& out, std::string& error, const std::string& coalesceTag)
{
    out = {};
    if (!Validate(document, changes, error)) return false;

    for (SolidChange& change : changes) ApplyForward(document, change, out);

    if (tryCoalesce(changes, coalesceTag))
    {
        error.clear();
        return true;
    }

    // Новая правка отбрасывает ветку redo; если в ней было сохранённое состояние, оно теперь недостижимо
    m_edits.resize(m_cursor);
    if (m_saved != kNoMark && m_saved > m_cursor) m_saved = kNoMark;

    m_edits.push_back({ std::move(name), std::move(changes), coalesceTag });
    ++m_cursor;

    if (m_edits.size() > kMaxEdits)
    {
        m_edits.erase(m_edits.begin());
        --m_cursor;
        if (m_saved != kNoMark) m_saved = (m_saved == 0) ? kNoMark : m_saved - 1;
    }

    error.clear();
    return true;
}

bool LevelHistory::undo(LevelDocument& document, ChangeSet& out)
{
    out = {};
    if (!canUndo()) return false;

    Edit& edit = m_edits[m_cursor - 1];
    edit.tag.clear();
    for (auto it = edit.changes.rbegin(); it != edit.changes.rend(); ++it) ApplyBackward(document, *it, out);

    --m_cursor;
    return true;
}

bool LevelHistory::redo(LevelDocument& document, ChangeSet& out)
{
    out = {};
    if (!canRedo()) return false;

    Edit& edit = m_edits[m_cursor];
    edit.tag.clear();
    for (SolidChange& change : edit.changes) ApplyForward(document, change, out);

    ++m_cursor;
    return true;
}

const std::string& LevelHistory::undoName() const
{
    static const std::string empty;
    return canUndo() ? m_edits[m_cursor - 1].name : empty;
}

const std::string& LevelHistory::redoName() const
{
    static const std::string empty;
    return canRedo() ? m_edits[m_cursor].name : empty;
}

void LevelHistory::clear()
{
    m_edits.clear();
    m_cursor = 0;
    m_saved = 0;
}
