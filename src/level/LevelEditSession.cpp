//
//  LevelEditSession.cpp
//  Cucumber
//

#include "LevelEditSession.h"

#include "BlockTool.h"
#include "LevelClipboard.h"
#include "SelectTool.h"
#include "VertexTool.h"

#include <algorithm>
#include <map>

LevelEditSession::LevelEditSession(LevelDocument& document, ChangedCallback onChanged)
    : m_document(document), m_onChanged(std::move(onChanged))
{
    m_tools[static_cast<size_t>(ToolType::Select)] = std::make_unique<SelectTool>();
    m_tools[static_cast<size_t>(ToolType::Block)] = std::make_unique<BlockTool>();
    m_tools[static_cast<size_t>(ToolType::Vertex)] = std::make_unique<VertexTool>();
}

LevelEditSession::~LevelEditSession() = default;

double LevelEditSession::gridSize() const
{
    return m_document.settings.gridSnap > 0.0f ? static_cast<double>(m_document.settings.gridSnap) : 1.0;
}

void LevelEditSession::notify(const ChangeSet& changes)
{
    pruneSelection();
    m_hasHover = false;   // индексы в hover могли устареть
    if (!changes.empty() && m_onChanged) m_onChanged(changes);
}

bool LevelEditSession::commit(std::string name, std::vector<SolidChange> changes, const std::string& coalesceTag)
{
    ChangeSet result;
    if (!m_history.commit(m_document, std::move(name), std::move(changes), result, m_lastError, coalesceTag)) return false;

    m_lastError.clear();
    notify(result);
    return true;
}

bool LevelEditSession::undo()
{
    if (isToolBusy()) return false;
    ChangeSet result;
    if (!m_history.undo(m_document, result)) return false;
    notify(result);
    return true;
}

bool LevelEditSession::redo()
{
    if (isToolBusy()) return false;
    ChangeSet result;
    if (!m_history.redo(m_document, result)) return false;
    notify(result);
    return true;
}

bool LevelEditSession::deleteSelection()
{
    if (isToolBusy()) return false;

    std::vector<SolidChange> changes;
    for (uint64_t uid : m_selectedSolids)
    {
        if (const Solid* solid = findSolid(uid)) changes.push_back(SolidChange::Removed(*solid));
    }
    if (changes.empty()) return false;

    const char* name = changes.size() == 1 ? "Delete solid" : "Delete solids";   // до перемещения changes: порядок вычисления аргументов не определён

    return commit(name, std::move(changes));
}

bool LevelEditSession::translateSelection(const glm::vec3& delta, bool textureLock, const std::string& coalesceTag)
{
    if (delta == glm::vec3(0.0f) || isToolBusy()) return false;

    std::vector<SolidChange> changes;
    for (uint64_t uid : m_selectedSolids)
    {
        if (const Solid* solid = findSolid(uid))
            changes.push_back(SolidChange::Modified(*solid, TranslatedSolid(*solid, delta, textureLock)));
    }
    if (changes.empty()) return false;

    const char* name = changes.size() == 1 ? "Move solid" : "Move solids";   // до перемещения changes: порядок вычисления аргументов не определён

    return commit(name, std::move(changes), coalesceTag);
}

AABB LevelEditSession::selectionBounds() const
{
    AABB box;
    for (uint64_t uid : m_selectedSolids)
    {
        if (const Solid* solid = findSolid(uid)) box.expand(SolidBounds(*solid));
    }
    return box;
}

bool LevelEditSession::duplicateSelection(const glm::vec3& offset, const std::string& coalesceTag)
{
    if (isToolBusy() || m_selectedSolids.empty()) return false;

    std::vector<SolidChange> changes;
    std::vector<uint64_t> copies;
    for (uint64_t uid : m_selectedSolids)
    {
        const Solid* solid = findSolid(uid);
        if (!solid) continue;

        Solid copy = TranslatedSolid(CloneWithNewUids(*solid), offset, true);
        copies.push_back(copy.uid);
        changes.push_back(SolidChange::Created(std::move(copy)));
    }
    if (changes.empty()) return false;

    const char* name = changes.size() == 1 ? "Duplicate solid" : "Duplicate solids";   // до перемещения changes: порядок вычисления аргументов не определён

    if (!commit(name, std::move(changes), coalesceTag)) return false;

    m_selectedFaces.clear();
    m_selectedSolids = std::move(copies);   // выбраны копии: дальше пользователь двигает именно их
    return true;
}

bool LevelEditSession::copySelection()
{
    std::vector<const Solid*> solids;
    for (uint64_t uid : m_selectedSolids)
    {
        if (const Solid* solid = findSolid(uid)) solids.push_back(solid);
    }
    if (solids.empty()) return false;

    LevelClipboard::Get().assign(m_document, solids);
    return true;
}

bool LevelEditSession::cutSelection()
{
    if (isToolBusy() || !copySelection()) return false;
    return deleteSelection();
}

bool LevelEditSession::paste()
{
    LevelClipboard& clipboard = LevelClipboard::Get();
    if (isToolBusy() || clipboard.empty()) return false;

    const AABB box = clipboard.bounds();
    const double grid = gridSize();
    const glm::dvec3 anchor((box.min.x + box.max.x) * 0.5, box.min.y, (box.min.z + box.max.z) * 0.5);   // центр основания

    glm::dvec3 delta(grid, 0.0, grid);   // без курсора: рядом с оригиналом

    if (m_hasLastPointer)
    {
        const Edit::Ray ray = MakeRay(m_lastPointer);
        bool found = false;
        glm::dvec3 aim(0.0);

        if (m_lastPointer.camera.ortho)
        {
            // Глубина (ось взгляда) остаётся прежней, курсор задаёт положение в плоскости вида
            const int axis = Edit::DominantAxis(glm::dvec3(m_lastPointer.camera.forward));
            glm::dvec3 normal(0.0);
            normal[axis] = 1.0;

            double t = 0.0;
            if (Edit::RayPlane(ray, anchor, normal, t))
            {
                aim = ray.at(t);
                glm::dvec3 target = Edit::SnapVec(aim, grid);
                target[axis] = anchor[axis];
                delta = target - anchor;
                found = true;
            }
        }
        else
        {
            LevelHit hit;
            double t = 0.0;
            if (RaycastLevel(m_document, glm::vec3(ray.origin), glm::vec3(ray.dir), static_cast<float>(ray.maxDistance), hit))
            {
                aim = glm::dvec3(hit.position);
                found = true;
            }
            else if (Edit::RayPlane(ray, glm::dvec3(0.0), glm::dvec3(0.0, 1.0, 0.0), t))
            {
                aim = ray.at(t);   // "земля" y = 0, как у инструмента Block
                found = true;
            }
            if (found) delta = Edit::SnapVec(aim, grid) - anchor;
        }
    }

    std::vector<Solid> copies = clipboard.instantiate(m_document, glm::vec3(delta), true);

    std::vector<SolidChange> changes;
    std::vector<uint64_t> uids;
    for (Solid& copy : copies)
    {
        uids.push_back(copy.uid);
        changes.push_back(SolidChange::Created(std::move(copy)));
    }

    const char* name = changes.size() == 1 ? "Paste solid" : "Paste solids";   // до перемещения changes: порядок вычисления аргументов не определён

    if (!commit(name, std::move(changes))) return false;

    m_selectedFaces.clear();
    m_selectedSolids = std::move(uids);
    return true;
}

bool LevelEditSession::handleKey(LevelKey key, bool shift)
{
    if (activeTool().onKey(*this, key, shift)) return true;
    if (isToolBusy()) return false;

    switch (key)
    {
        case LevelKey::Copy:      return copySelection();
        case LevelKey::Cut:       return cutSelection();
        case LevelKey::Paste:     return paste();
        case LevelKey::Duplicate: return duplicateSelection(glm::vec3(static_cast<float>(gridSize()), 0.0f, static_cast<float>(gridSize())));

        case LevelKey::DuplicateMove:
            // Работает из любого инструмента: копию двигает инструмент Select
            if (m_selectedSolids.empty() || m_active == ToolType::Select || !m_hasLastPointer) return false;
            setTool(ToolType::Select);
            {
                // Select не видел курсор, пока был активен другой инструмент: сообщаем ему положение (без нажатий кнопок)
                ViewportPointer pointer = m_lastPointer;
                pointer.pressed = pointer.down = pointer.released = false;
                activeTool().onPointer(*this, m_lastPointerView, pointer);
            }
            return activeTool().onKey(*this, key, shift);

        default:
            return false;
    }
}

bool LevelEditSession::isSolidSelected(uint64_t uid) const
{
    return std::find(m_selectedSolids.begin(), m_selectedSolids.end(), uid) != m_selectedSolids.end();
}

void LevelEditSession::clearSelection()
{
    m_selectedSolids.clear();
    m_selectedFaces.clear();
}

void LevelEditSession::selectSolid(uint64_t uid, bool toggle)
{
    m_selectedFaces.clear();

    const auto it = std::find(m_selectedSolids.begin(), m_selectedSolids.end(), uid);
    if (toggle)
    {
        if (it != m_selectedSolids.end()) m_selectedSolids.erase(it);
        else                              m_selectedSolids.push_back(uid);
        return;
    }

    m_selectedSolids.assign(1, uid);
}

bool LevelEditSession::isFaceSelected(uint64_t faceUid) const
{
    return std::any_of(m_selectedFaces.begin(), m_selectedFaces.end(), [&](const FaceRef& f) { return f.faceUid == faceUid; });
}

void LevelEditSession::selectFace(uint64_t solidUid, uint64_t faceUid, bool toggle)
{
    m_selectedSolids.clear();

    const FaceRef ref{ solidUid, faceUid };
    const auto it = std::find(m_selectedFaces.begin(), m_selectedFaces.end(), ref);
    if (toggle)
    {
        if (it != m_selectedFaces.end()) m_selectedFaces.erase(it);
        else                             m_selectedFaces.push_back(ref);
        return;
    }

    m_selectedFaces.assign(1, ref);
}

const Face* LevelEditSession::findFace(const FaceRef& ref) const
{
    const Solid* solid = findSolid(ref.solidUid);
    if (!solid) return nullptr;

    for (const Face& face : solid->faces)
    {
        if (face.uid == ref.faceUid) return &face;
    }
    return nullptr;
}

std::vector<FaceRef> LevelEditSession::textureTargets() const
{
    if (!m_selectedFaces.empty()) return m_selectedFaces;

    std::vector<FaceRef> targets;
    for (uint64_t uid : m_selectedSolids)
    {
        const Solid* solid = findSolid(uid);
        if (!solid) continue;
        for (const Face& face : solid->faces) targets.push_back({ uid, face.uid });
    }
    return targets;
}

bool LevelEditSession::editFaces(const std::vector<FaceRef>& faces, std::string name, std::string coalesceTag, const FaceEditFn& fn)
{
    if (isToolBusy()) return false;

    std::vector<uint64_t> solidOrder;   // тела в порядке первого упоминания, без повторов
    for (const FaceRef& ref : faces)
    {
        if (std::find(solidOrder.begin(), solidOrder.end(), ref.solidUid) == solidOrder.end()) solidOrder.push_back(ref.solidUid);
    }

    std::vector<SolidChange> changes;
    for (uint64_t solidUid : solidOrder)
    {
        const Solid* original = findSolid(solidUid);
        if (!original) continue;

        Solid edited = *original;
        bool changed = false;

        for (const FaceRef& ref : faces)
        {
            if (ref.solidUid != solidUid) continue;

            for (Face& face : edited.faces)
            {
                if (face.uid != ref.faceUid) continue;

                const Face before = face;
                const bool reported = fn(*original, face);
                const bool differs = before.material != face.material || before.flags != face.flags ||
                                     before.uv.uAxis != face.uv.uAxis || before.uv.vAxis != face.uv.vAxis ||
                                     before.uv.shift != face.uv.shift || before.uv.scale != face.uv.scale;
                changed = changed || (reported && differs);
            }
        }
        if (changed) changes.push_back(SolidChange::Modified(*original, std::move(edited)));
    }

    if (changes.empty()) return false;
    return commit(std::move(name), std::move(changes), coalesceTag);
}

const Solid* LevelEditSession::findSolid(uint64_t uid) const
{
    for (const Solid& solid : m_document.solids)
    {
        if (solid.uid == uid) return &solid;
    }
    return nullptr;
}

void LevelEditSession::pruneSelection()
{
    m_selectedSolids.erase(std::remove_if(m_selectedSolids.begin(), m_selectedSolids.end(),
                           [&](uint64_t uid) { return findSolid(uid) == nullptr; }), m_selectedSolids.end());

    m_selectedFaces.erase(std::remove_if(m_selectedFaces.begin(), m_selectedFaces.end(),
                          [&](const FaceRef& ref) { return findFace(ref) == nullptr; }), m_selectedFaces.end());
}

uint32_t LevelEditSession::currentMaterial()
{
    const auto faceMaterial = [&](const Solid& solid, uint64_t faceUid) -> int64_t
    {
        for (const Face& face : solid.faces)
        {
            if (face.uid == faceUid) return face.material;
        }
        return -1;
    };

    for (const FaceRef& ref : m_selectedFaces)
    {
        if (const Solid* solid = findSolid(ref.solidUid))
        {
            const int64_t material = faceMaterial(*solid, ref.faceUid);
            if (material >= 0) return static_cast<uint32_t>(material);
        }
    }

    // Самый частый материал среди видимых граней (выделенных солидов, если они есть, иначе всего документа)
    std::map<uint32_t, int> counts;
    const auto count = [&](const Solid& solid)
    {
        for (const Face& face : solid.faces)
        {
            if (!(face.flags & FaceFlag::NoDraw)) ++counts[face.material];
        }
    };

    for (uint64_t uid : m_selectedSolids)
    {
        if (const Solid* solid = findSolid(uid)) count(*solid);
    }
    if (counts.empty())
    {
        for (const Solid& solid : m_document.solids) count(solid);
    }

    uint32_t best = 0;
    int bestCount = 0;
    for (const auto& [material, n] : counts)
    {
        if (n > bestCount) { best = material; bestCount = n; }
    }
    if (bestCount > 0) return best;

    // Подсказок нет: первый материал таблицы, а в пустой таблице заводим "default"
    return m_document.materials.empty() ? m_document.materialIndex("default") : 0;
}

void LevelEditSession::setTool(ToolType type)
{
    if (type == m_active) return;

    activeTool().cancel();
    m_active = type;
    m_hasHover = false;
}

bool LevelEditSession::escape()
{
    if (activeTool().isBusy())
    {
        activeTool().cancel();
        return true;
    }
    if (m_active != ToolType::Select)
    {
        setTool(ToolType::Select);
        return true;
    }
    if (!m_selectedSolids.empty() || !m_selectedFaces.empty())
    {
        clearSelection();
        return true;
    }
    return false;
}

void LevelEditSession::updateHover(uint32_t viewId, const ViewportPointer& pointer)
{
    if (!pointer.over)
    {
        if (m_hasHover && m_hoverView == viewId) m_hasHover = false;
        return;
    }

    m_hoverView = viewId;
    const Edit::Ray ray = MakeRay(pointer);
    m_hasHover = RaycastLevel(m_document, glm::vec3(ray.origin), glm::vec3(ray.dir), static_cast<float>(ray.maxDistance), m_hover);
}

void LevelEditSession::handlePointer(uint32_t viewId, const ViewportPointer& pointer)
{
    if (pointer.over && !pointer.capturing)
    {
        m_lastPointer = pointer;
        m_lastPointerView = viewId;
        m_hasLastPointer = true;
    }
    else if (m_hasLastPointer && m_lastPointerView == viewId)
    {
        m_hasLastPointer = false;   // курсор ушёл с вида, на котором его видели: вставка по курсору недоступна
    }

    if (m_active == ToolType::Select && !isToolBusy()) updateHover(viewId, pointer);
    else                                               m_hasHover = false;

    activeTool().onPointer(*this, viewId, pointer);
}
