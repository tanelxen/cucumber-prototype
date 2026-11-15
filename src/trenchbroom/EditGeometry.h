//
//  EditGeometry.h
//  Cucumber
//
//  Геометрия уровня для режима правки: без лайтмапы, обновляется ИНКРЕМЕНТАЛЬНО. Без OpenGL (заливкой в GPU занят
//  LevelEditMesh), поэтому проверяется тестами.
//
//  Устройство:
//    - кэш по солидам: треугольники каждого солида, разложенные по материалам (кусок "солид x материал");
//    - чанки = (ячейка 1024 x материал), как у кукера: те же поверхности, то же отсечение, те же вызовы отрисовки;
//    - плоские массивы вершин и индексов, в которых чанки лежат подряд (индексы абсолютные).
//
//  Правка перегенерирует только изменённые солиды. Если число вершин и индексов затронутых чанков не изменилось
//  (сдвиг в пределах ячейки, push/pull, UV, правка вершин без распада граней), раскладка остаётся прежней, и
//  UpdateResult называет ровно те диапазоны, которые надо перелить в GPU. Иначе раскладка пересчитывается из кэша
//  (без повторного приготовления геометрии) и в GPU льётся всё целиком.
//
//  Содержимое чанка совпадает с тем, что даёт LevelCooker (режим игры): те же вершины, нормали, UV и порядок обхода.
//

#pragma once

#include "Bounds.h"
#include "level/LevelDocument.h"
#include "level/LevelHistory.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

class EditGeometry
{
public:
    // Размер текстуры материала (для UV). Вызывается лениво, результат кэшировать на стороне вызывающего.
    using SizeResolver = std::function<glm::ivec2(const std::string& materialName)>;

    // Как в VertexShading: нормаль и UV подряд, без выравнивания (заливается в GPU как есть)
    struct Shading
    {
        glm::vec3 normal;
        glm::vec2 texCoord;
    };

    struct Chunk
    {
        int cellX = 0, cellY = 0, cellZ = 0;
        std::string material;

        std::vector<uint64_t> solids;      // uid солидов, у которых в этой ячейке есть грани этого материала
        uint32_t vertexStart = 0, vertexCount = 0;
        uint32_t indexStart = 0, indexCount = 0;
        AABB bounds;
    };

    struct Range
    {
        uint32_t start = 0;
        uint32_t count = 0;
    };

    struct UpdateResult
    {
        bool changed = false;          // что-то изменилось (ревизию меша надо поднять)
        bool layoutChanged = false;    // чанки появились / исчезли / поменяли размер: раскладка сдвинулась
        bool boundsChanged = false;    // поменялись границы чанков (поверхности надо пересобрать)

        // При !layoutChanged: диапазоны вершин (позиции и затенение) и индексов, изменённые правкой
        std::vector<Range> vertexRanges;
        std::vector<Range> indexRanges;

        // При layoutChanged: всё до начала хвоста осталось на прежних местах и прежним, перелить надо только хвост
        // (до конца массива после правки). Сдвиг чанков идёт вправо от первого изменившегося, поэтому при правке
        // "в середине" хвост короче полного массива. После rebuild хвост это весь массив.
        // GPU-копия при этом должна быть подогнана под новый размер массивов (хвост покрывает прирост).
        Range vertexTail;
        Range indexTail;
    };

    // Всё заново (вход в режим правки, перезагрузка документа)
    UpdateResult rebuild(const LevelDocument& level, const SizeResolver& sizeOf);

    // Применяет изменения документа (после LevelEditSession::commit / undo / redo)
    UpdateResult update(const LevelDocument& level, const ChangeSet& changes, const SizeResolver& sizeOf);

    const std::vector<glm::vec3>& positions() const { return m_positions; }
    const std::vector<Shading>& shading() const { return m_shading; }
    const std::vector<uint32_t>& indices() const { return m_indices; }

    // Чанки в порядке отрисовки (ячейка, затем материал); указатели действительны до следующего изменения набора чанков
    const std::vector<const Chunk*>& chunks() const { return m_order; }

private:
    // Треугольники солида по материалам. Индексы локальные (с нуля внутри куска).
    struct Piece
    {
        std::vector<glm::vec3> positions;
        std::vector<Shading> shading;
        std::vector<uint32_t> indices;
        AABB bounds;
    };

    struct SolidGeometry
    {
        int cellX = 0, cellY = 0, cellZ = 0;
        std::map<std::string, Piece> pieces;     // по имени материала
    };

    using ChunkKey = std::tuple<int, int, int, std::string>;

    static SolidGeometry BuildSolid(const LevelDocument& level, const Solid& solid, const SizeResolver& sizeOf);

    void addToChunks(uint64_t uid, const SolidGeometry& geometry, std::vector<ChunkKey>* dirty);
    void removeFromChunks(uint64_t uid, const SolidGeometry& geometry, std::vector<ChunkKey>* dirty);
    void refreshChunk(Chunk& chunk) const;                 // число вершин / индексов и границы по текущим членам
    void writeChunk(const Chunk& chunk);                   // перезаписывает диапазоны чанка в массивах
    using StartMap = std::map<ChunkKey, std::pair<uint32_t, uint32_t>>;   // прежние (vertexStart, indexStart) по чанкам

    // Раскладка всех чанков подряд. Без oldStarts переписывает массивы целиком. С oldStarts и списком изменённых чанков
    // переписывает только изменённые и сдвинувшиеся: остальные уже лежат на своих местах. tailVertex / tailIndex
    // получают начало хвоста (первый переписанный чанк) или конец массивов, если переписывать нечего.
    void relayout(const StartMap* oldStarts, const std::vector<ChunkKey>* dirty, uint32_t* tailVertex, uint32_t* tailIndex);

    std::unordered_map<uint64_t, SolidGeometry> m_solids;
    std::map<ChunkKey, Chunk> m_chunks;
    std::vector<const Chunk*> m_order;

    std::vector<glm::vec3> m_positions;
    std::vector<Shading> m_shading;
    std::vector<uint32_t> m_indices;
};
