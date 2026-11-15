//
//  LevelEditMesh.cpp
//  Cucumber
//

#include "LevelEditMesh.h"

#include "GLState.h"
#include "GeometryRevision.h"
#include "IndexBuffer.h"
#include "TextureLibrary.h"
#include "TextureManager.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>
#include <iostream>

namespace
{
    static_assert(sizeof(EditGeometry::Shading) == 5 * sizeof(float), "Shading must be tightly packed for GPU upload");
}

LevelEditMesh::LevelEditMesh() = default;

LevelEditMesh::~LevelEditMesh()
{
    // VAO и буферы освобождают свои деструкторы
    if (m_whiteTexture != 0)
    {
        const GLuint texture = m_whiteTexture;
        glDeleteTextures(1, &texture);
        GLState::invalidate();
    }
}

unsigned int LevelEditMesh::vaoId() const
{
    return m_vao ? m_vao->getID() : 0;
}

EditGeometry::SizeResolver LevelEditMesh::sizeResolver()
{
    return [](const std::string& name)
    {
        const auto& info = TextureLibrary::Get().info(name);
        return glm::ivec2(info.width, info.height);
    };
}

void LevelEditMesh::createGpuObjects()
{
    // Раскладка совпадает с MapMesh: те же три потока атрибутов
    m_positionBuffer = std::make_unique<VertexBuffer>();
    m_positionBuffer->setLayout({
        { VertexAttributeLocation::Position, ShaderDataType::Float3 }
    });

    m_shadingBuffer = std::make_unique<VertexBuffer>();
    m_shadingBuffer->setLayout({
        { VertexAttributeLocation::Normal, ShaderDataType::Float3 },
        { VertexAttributeLocation::TexCoord, ShaderDataType::Float2 }
    });

    m_lightmapBuffer = std::make_unique<VertexBuffer>(BufferUsage::Static);
    m_lightmapBuffer->setLayout({
        { VertexAttributeLocation::TexCoord2, ShaderDataType::Float2 }
    });

    m_indexBuffer = std::make_unique<IndexBuffer>();

    // VAO запоминает id буферов, а не их хранилище: дальнейший рост через glBufferData его не ломает
    m_vao = std::make_unique<VertexArray>();
    m_vao->setVertexBuffer(m_positionBuffer.get());
    m_vao->setVertexBuffer(m_shadingBuffer.get());
    m_vao->setVertexBuffer(m_lightmapBuffer.get());
    m_vao->setIndexBuffer(m_indexBuffer.get());

    GLState::invalidate();
}

void LevelEditMesh::uploadWhiteTexture()
{
    // Лайтмапы в режиме правки нет: белая 1x1 даёт постоянный множитель.
    // TODO: у MapMesh до запекания белая текстура 1.0 (0x3C00), здесь 2.0 (0x4000). Проверить, намеренно ли,
    // и выделить одну общую "нейтральную лайтмапу".
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const uint16_t white[3] = { 0x4000, 0x4000, 0x4000 };   // 2.0 в half float
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 1, 1, 0, GL_RGB, GL_HALF_FLOAT, white);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_whiteTexture = texture;
    GLState::invalidate();
}

void LevelEditMesh::ensureLightmapBuffer()
{
    // Нули на каждую вершину ёмкости: вершинный буфер мог вырасти
    const size_t vertexCapacity = m_positionBuffer->capacity() / sizeof(glm::vec3);
    if (!m_lightmapBuffer->reserve(vertexCapacity * sizeof(glm::vec2))) return;

    // Хранилище пересоздано, его содержимое не определено
    const std::vector<float> zeros(m_lightmapBuffer->capacity() / sizeof(float), 0.0f);
    m_lightmapBuffer->write(0, zeros.data(), zeros.size() * sizeof(float));
}

void LevelEditMesh::rebuildSurfaces()
{
    m_surfaces.clear();
    m_surfaceBounds.clear();

    for (const EditGeometry::Chunk* chunk : m_geometry.chunks())
    {
        Surface surface;
        surface.m_name = "surface_" + chunk->material;
        surface.materialSlot = m_materials.slot(chunk->material);
        surface.startIndex = chunk->indexStart;
        surface.count = chunk->indexCount;

        m_surfaces.push_back(std::move(surface));
        m_surfaceBounds.push_back(chunk->bounds);
    }

    m_stats.surfaces = m_surfaces.size();
}

void LevelEditMesh::upload(const EditGeometry::UpdateResult& result)
{
    if (!result.changed) return;

    if (!m_vao) createGpuObjects();
    if (m_whiteTexture == 0) uploadWhiteTexture();

    const auto& positions = m_geometry.positions();
    const auto& shading = m_geometry.shading();
    const auto& indices = m_geometry.indices();

    size_t uploaded = 0;

    if (result.layoutChanged)
    {
        // Раскладка сдвинулась: все чанки до первого изменившегося остались на месте, льём только хвост
        uploaded += m_positionBuffer->writeTail(positions.data(), positions.size(), sizeof(glm::vec3), result.vertexTail.start);
        uploaded += m_shadingBuffer->writeTail(shading.data(), shading.size(), sizeof(EditGeometry::Shading), result.vertexTail.start);
        uploaded += m_indexBuffer->writeTail(indices.data(), indices.size(), sizeof(uint32_t), result.indexTail.start);
        ensureLightmapBuffer();
    }
    else
    {
        // Раскладка прежняя: переливаем только затронутые чанки
        for (const EditGeometry::Range& range : result.vertexRanges)
        {
            m_positionBuffer->write(range.start * sizeof(glm::vec3), positions.data() + range.start, range.count * sizeof(glm::vec3));
            m_shadingBuffer->write(range.start * sizeof(EditGeometry::Shading), shading.data() + range.start, range.count * sizeof(EditGeometry::Shading));
            uploaded += range.count * (sizeof(glm::vec3) + sizeof(EditGeometry::Shading));
        }
        for (const EditGeometry::Range& range : result.indexRanges)
        {
            m_indexBuffer->write(range.start * sizeof(uint32_t), indices.data() + range.start, range.count * sizeof(uint32_t));
            uploaded += range.count * sizeof(uint32_t);
        }
    }

    m_indexBuffer->setCount(static_cast<uint32_t>(indices.size()));

    // Буферы больше не привязываются к своим целям, так что кэшу GLState здесь, скорее всего, ничего не грозит.
    // Вызов оставлен для надёжности (раз в коммит правки); уберите, если GLState не кэширует буферы.
    GLState::invalidate();

    if (result.layoutChanged || result.boundsChanged) rebuildSurfaces();

    m_stats.vertices = positions.size();
    m_stats.indices = indices.size();
    m_stats.lastUploadBytes = uploaded;
    m_stats.lastLayoutChanged = result.layoutChanged;

    m_revision = NextGeometryRevision();   // RenderScene перестроит статику (границы поверхностей могли поменяться)
}

void LevelEditMesh::build(const LevelDocument& level)
{
    m_materials.clear();

    upload(m_geometry.rebuild(level, sizeResolver()));

    std::cout << "[LevelEditMesh] Built edit geometry. Vertices: " << m_stats.vertices << ", surfaces: " << m_stats.surfaces << std::endl;
}

void LevelEditMesh::apply(const LevelDocument& level, const ChangeSet& changes)
{
    upload(m_geometry.update(level, changes, sizeResolver()));
}
