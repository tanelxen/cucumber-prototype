//
//  LevelEditSession.cpp
//  Cucumber
//

#include "LevelEditSession.h"

#include "BlockTool.h"
#include "SelectTool.h"
#include "VertexTool.h"

#include <algorithm>
#include <map>

Solid TranslatedSolid(const Solid& solid, const glm::vec3& delta, bool textureLock)
{
    Solid result = solid;
    for (glm::vec3& v : result.vertices) v += delta;

    if (textureLock)
    {
        // u = dot(pos, uAxis) / scale.x + shift.x: после сдвига pos на delta u уходит на dot(delta, uAxis) / scale.x.
        // Вычитаем это из shift, и текстура остаётся на теле.
        for (Face& face : result.faces)
        {
            const float sx = face.uv.scale.x != 0.0f ? face.uv.scale.x : 1.0f;
            const float sy = face.uv.scale.y != 0.0f ? face.uv.scale.y : 1.0f;
            face.uv.shift.x -= glm::dot(delta, face.uv.uAxis) / sx;
            face.uv.shift.y -= glm::dot(delta, face.uv.vAxis) / sy;
        }
    }
    return result;
}

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

    return commit(changes.size() == 1 ? "Delete solid" : "Delete solids", std::move(changes));
}

bool LevelEditSession::translateSelection(const glm::vec3& delta, bool textureLock)
{
    if (delta == glm::vec3(0.0f) || isToolBusy()) return false;

    std::vector<SolidChange> changes;
    for (uint64_t uid : m_selectedSolids)
    {
        if (const Solid* solid = findSolid(uid))
            changes.push_back(SolidChange::Modified(*solid, TranslatedSolid(*solid, delta, textureLock)));
    }
    if (changes.empty()) return false;

    return commit(changes.size() == 1 ? "Move solid" : "Move solids", std::move(changes));
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
    if (m_active == ToolType::Select && !isToolBusy()) updateHover(viewId, pointer);
    else                                               m_hasHover = false;

    activeTool().onPointer(*this, viewId, pointer);
}
