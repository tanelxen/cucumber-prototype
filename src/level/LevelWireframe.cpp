//
//  LevelWireframe.cpp
//  Cucumber
//

#include "LevelWireframe.h"

#include "MapComponent.h"
#include "level/LevelDocument.h"
#include "trenchbroom/LevelEditMesh.h"
#include "trenchbroom/MapMesh.h"

namespace
{
    // Толщина в логических пикселях: на Retina линия выглядит так же, как на обычном экране
    constexpr float kWireWidthPx = 1.0f;

    const glm::vec4 kWireColor{0.78f, 0.78f, 0.82f, 1.0f};
}

LevelWireframe::~LevelWireframe()
{
    release();
}

void LevelWireframe::release()
{
    if (m_set != 0)
    {
        OverlayRender::Get().DestroyLineSet(m_set);
        m_set = 0;
    }
    m_document = nullptr;
    m_built = false;
}

void LevelWireframe::Collect(const LevelDocument& level, std::vector<OverlayRender::StaticLine>& out)
{
    out.clear();

    size_t estimate = 0;
    for (const Solid& solid : level.solids) estimate += solid.faces.size() * 2;
    out.reserve(estimate);

    for (const Solid& solid : level.solids)
    {
        const uint32_t vertexCount = static_cast<uint32_t>(solid.vertices.size());

        for (const Face& face : solid.faces)
        {
            const size_t count = face.loop.size();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t a = face.loop[i];
                const uint32_t b = face.loop[(i + 1) % count];

                // Ребро замкнутого тела делят две грани в противоположных направлениях: берём его один раз
                // (так же, как ToolPreview::addSolidEdges)
                if (a >= b || b >= vertexCount) continue;

                out.push_back({ solid.vertices[a], solid.vertices[b], kWireColor });
            }
        }
    }
}

uint32_t LevelWireframe::sync(MapComponent* map)
{
    if (!map) return 0;

    // Та же ревизия, по которой SceneExtractor перестраивает статику: у режима правки свой счётчик
    // (LevelEditMesh), вне правки ревизия MapMesh. Они не пересекаются (у правки выставлен отдельный бит).
    const LevelEditMesh* editMesh = map->editMesh();
    const uint64_t revision = editMesh ? editMesh->getRevision() : (map->mesh ? map->mesh->getRevision() : 0);

    if (m_set == 0) m_set = OverlayRender::Get().CreateLineSet();
    if (m_set == 0) return 0;   // OverlayRender ещё не инициализирован

    if (!m_built || m_document != &map->level || m_revision != revision)
    {
        std::vector<OverlayRender::StaticLine> lines;
        Collect(map->level, lines);
        OverlayRender::Get().UpdateLineSet(m_set, lines, kWireWidthPx);

        m_document = &map->level;
        m_revision = revision;
        m_built = true;
    }

    return m_set;
}
