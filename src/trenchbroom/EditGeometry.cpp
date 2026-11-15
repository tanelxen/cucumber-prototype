//
//  EditGeometry.cpp
//  Cucumber
//

#include "EditGeometry.h"

#include "CookCommon.h"

#include <algorithm>
#include <unordered_set>

namespace
{
    bool SameBounds(const AABB& a, const AABB& b)
    {
        return a.min == b.min && a.max == b.max;
    }
}

EditGeometry::SolidGeometry EditGeometry::BuildSolid(const LevelDocument& level, const Solid& solid, const SizeResolver& sizeOf)
{
    SolidGeometry geometry;
    if (solid.faces.empty()) return geometry;

    const CookCommon::Cell cell = CookCommon::CellOfSolid(solid);
    geometry.cellX = cell.x;
    geometry.cellY = cell.y;
    geometry.cellZ = cell.z;

    std::map<std::string, glm::ivec2> textureSizes;

    for (const Face& face : solid.faces)
    {
        if (face.flags & FaceFlag::NoDraw) continue;

        const std::string& material = level.materials[face.material];

        auto [sizeIt, inserted] = textureSizes.try_emplace(material);
        if (inserted) sizeIt->second = glm::max(sizeOf ? sizeOf(material) : glm::ivec2(256), glm::ivec2(1));

        const std::vector<glm::vec3> points = CookCommon::FacePoints(solid, face);
        const glm::vec3 normal = FaceNormal(solid, face);

        Piece& piece = geometry.pieces[material];
        const uint32_t first = static_cast<uint32_t>(piece.positions.size());

        for (const glm::vec3& point : points)
        {
            piece.positions.push_back(point);
            piece.shading.push_back({ normal, CookCommon::ValveUV(point, face.uv, sizeIt->second.x, sizeIt->second.y) });
            piece.bounds.expand(point);
        }

        // Выпуклая грань: веер из первой вершины
        for (size_t t = 1; t + 1 < points.size(); ++t)
        {
            piece.indices.push_back(first);
            piece.indices.push_back(first + static_cast<uint32_t>(t));
            piece.indices.push_back(first + static_cast<uint32_t>(t) + 1);
        }
    }

    // Грань без треугольников (меньше трёх вершин) куска не даёт
    for (auto it = geometry.pieces.begin(); it != geometry.pieces.end();)
    {
        it = it->second.indices.empty() ? geometry.pieces.erase(it) : std::next(it);
    }
    return geometry;
}

void EditGeometry::addToChunks(uint64_t uid, const SolidGeometry& geometry, std::vector<ChunkKey>* dirty)
{
    for (const auto& [material, piece] : geometry.pieces)
    {
        (void)piece;
        const ChunkKey key(geometry.cellX, geometry.cellY, geometry.cellZ, material);

        Chunk& chunk = m_chunks[key];
        chunk.cellX = geometry.cellX;
        chunk.cellY = geometry.cellY;
        chunk.cellZ = geometry.cellZ;
        chunk.material = material;
        chunk.solids.push_back(uid);

        if (dirty) dirty->push_back(key);
    }
}

void EditGeometry::removeFromChunks(uint64_t uid, const SolidGeometry& geometry, std::vector<ChunkKey>* dirty)
{
    for (const auto& [material, piece] : geometry.pieces)
    {
        (void)piece;
        const ChunkKey key(geometry.cellX, geometry.cellY, geometry.cellZ, material);

        const auto it = m_chunks.find(key);
        if (it == m_chunks.end()) continue;

        auto& members = it->second.solids;
        members.erase(std::remove(members.begin(), members.end(), uid), members.end());

        if (dirty) dirty->push_back(key);
    }
}

void EditGeometry::refreshChunk(Chunk& chunk) const
{
    chunk.vertexCount = 0;
    chunk.indexCount = 0;
    chunk.bounds = AABB();

    for (uint64_t uid : chunk.solids)
    {
        const Piece& piece = m_solids.at(uid).pieces.at(chunk.material);
        chunk.vertexCount += static_cast<uint32_t>(piece.positions.size());
        chunk.indexCount += static_cast<uint32_t>(piece.indices.size());
        chunk.bounds.expand(piece.bounds);
    }
}

void EditGeometry::writeChunk(const Chunk& chunk)
{
    uint32_t vertex = chunk.vertexStart;
    uint32_t index = chunk.indexStart;

    for (uint64_t uid : chunk.solids)
    {
        const Piece& piece = m_solids.at(uid).pieces.at(chunk.material);

        std::copy(piece.positions.begin(), piece.positions.end(), m_positions.begin() + vertex);
        std::copy(piece.shading.begin(), piece.shading.end(), m_shading.begin() + vertex);
        for (uint32_t local : piece.indices) m_indices[index++] = vertex + local;   // индексы абсолютные
        vertex += static_cast<uint32_t>(piece.positions.size());
    }
}

void EditGeometry::relayout(const StartMap* oldStarts, const std::vector<ChunkKey>* dirty, uint32_t* tailVertex, uint32_t* tailIndex)
{
    m_order.clear();

    uint32_t vertexTotal = 0;
    uint32_t indexTotal = 0;
    for (auto& [key, chunk] : m_chunks)
    {
        chunk.vertexStart = vertexTotal;
        chunk.indexStart = indexTotal;
        vertexTotal += chunk.vertexCount;
        indexTotal += chunk.indexCount;
        m_order.push_back(&chunk);
    }

    // resize сохраняет содержимое: чанки, оставшиеся на своих местах, перезаписывать не нужно
    m_positions.resize(vertexTotal);
    m_shading.resize(vertexTotal);
    m_indices.resize(indexTotal);

    if (tailVertex) *tailVertex = vertexTotal;
    if (tailIndex) *tailIndex = indexTotal;
    bool tailFound = false;

    for (const auto& [key, chunk] : m_chunks)
    {
        if (oldStarts)
        {
            const auto old = oldStarts->find(key);
            const bool isDirty = dirty && std::binary_search(dirty->begin(), dirty->end(), key);
            const bool moved = old == oldStarts->end() || old->second.first != chunk.vertexStart || old->second.second != chunk.indexStart;
            if (!isDirty && !moved) continue;
        }

        writeChunk(chunk);

        if (!tailFound)
        {
            if (tailVertex) *tailVertex = chunk.vertexStart;
            if (tailIndex) *tailIndex = chunk.indexStart;
            tailFound = true;
        }
    }
}

EditGeometry::UpdateResult EditGeometry::rebuild(const LevelDocument& level, const SizeResolver& sizeOf)
{
    m_solids.clear();
    m_chunks.clear();

    // Солиды в порядке документа: содержимое чанков тогда совпадает с кукером до последнего байта
    for (const Solid& solid : level.solids)
    {
        SolidGeometry geometry = BuildSolid(level, solid, sizeOf);
        addToChunks(solid.uid, geometry, nullptr);
        m_solids.emplace(solid.uid, std::move(geometry));
    }
    for (auto& [key, chunk] : m_chunks) refreshChunk(chunk);

    m_positions.clear();
    m_shading.clear();
    m_indices.clear();
    relayout(nullptr, nullptr, nullptr, nullptr);

    UpdateResult result;
    result.changed = true;
    result.layoutChanged = true;
    result.boundsChanged = true;
    result.vertexTail = { 0, static_cast<uint32_t>(m_positions.size()) };
    result.indexTail = { 0, static_cast<uint32_t>(m_indices.size()) };
    return result;
}

EditGeometry::UpdateResult EditGeometry::update(const LevelDocument& level, const ChangeSet& changes, const SizeResolver& sizeOf)
{
    UpdateResult result;
    if (changes.empty()) return result;
    result.changed = true;

    // Какие солиды перегенерировать: изменённые и добавленные (из документа), удалённые только убираем
    std::unordered_set<uint64_t> regenerate(changes.added.begin(), changes.added.end());
    regenerate.insert(changes.modified.begin(), changes.modified.end());

    std::unordered_map<uint64_t, const Solid*> current;   // один проход по документу вместо поиска на каждый uid
    if (!regenerate.empty())
    {
        for (const Solid& solid : level.solids)
        {
            if (regenerate.count(solid.uid)) current.emplace(solid.uid, &solid);
        }
    }

    std::vector<ChunkKey> dirty;

    const auto drop = [&](uint64_t uid)
    {
        const auto it = m_solids.find(uid);
        if (it == m_solids.end()) return;
        removeFromChunks(uid, it->second, &dirty);
        m_solids.erase(it);
    };

    for (uint64_t uid : changes.removed) drop(uid);

    for (uint64_t uid : regenerate)
    {
        drop(uid);

        const auto it = current.find(uid);
        if (it == current.end()) continue;   // в документе уже нет: считаем удалённым

        SolidGeometry geometry = BuildSolid(level, *it->second, sizeOf);
        addToChunks(uid, geometry, &dirty);
        m_solids.emplace(uid, std::move(geometry));
    }

    std::sort(dirty.begin(), dirty.end());
    dirty.erase(std::unique(dirty.begin(), dirty.end()), dirty.end());

    // Затронутые чанки: новые размеры и границы; решаем, остаётся ли раскладка прежней
    std::vector<const Chunk*> touched;
    for (const ChunkKey& key : dirty)
    {
        const auto it = m_chunks.find(key);
        if (it == m_chunks.end()) continue;

        Chunk& chunk = it->second;
        if (chunk.solids.empty())
        {
            m_chunks.erase(it);          // чанк опустел
            result.layoutChanged = true;
            continue;
        }

        const uint32_t oldVertices = chunk.vertexCount;
        const uint32_t oldIndices = chunk.indexCount;
        const AABB oldBounds = chunk.bounds;

        refreshChunk(chunk);

        if (chunk.vertexCount != oldVertices || chunk.indexCount != oldIndices) result.layoutChanged = true;   // в том числе новый чанк (было 0)
        if (!SameBounds(chunk.bounds, oldBounds)) result.boundsChanged = true;
        touched.push_back(&chunk);
    }

    if (result.layoutChanged)
    {
        // Чанки до первого изменившегося (изменённого или сдвинувшегося) лежат там же и теми же: хвост начинается с него
        StartMap oldStarts;
        for (const auto& [key, chunk] : m_chunks) oldStarts.emplace(key, std::make_pair(chunk.vertexStart, chunk.indexStart));

        uint32_t tailVertex = 0, tailIndex = 0;
        relayout(&oldStarts, &dirty, &tailVertex, &tailIndex);
        result.boundsChanged = true;

        result.vertexTail = { tailVertex, static_cast<uint32_t>(m_positions.size()) - tailVertex };
        result.indexTail = { tailIndex, static_cast<uint32_t>(m_indices.size()) - tailIndex };
        return result;
    }

    // Раскладка прежняя: перезаписываем только затронутые чанки и сообщаем диапазоны
    for (const Chunk* chunk : touched)
    {
        writeChunk(*chunk);
        result.vertexRanges.push_back({ chunk->vertexStart, chunk->vertexCount });
        result.indexRanges.push_back({ chunk->indexStart, chunk->indexCount });
    }
    return result;
}
