//
//  LevelEditMesh.cpp
//  Cucumber
//

#include "LevelEditMesh.h"

#include "GLState.h"
#include "Q3Shaders.h"
#include "TextureManager.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>
#include <iostream>

namespace
{
    // Скрипты шейдеров разбираются один раз на путь: initFromDir читает каталог, а правок за сеанс сотни
    Quake3Shaders s_shaders;
    std::string s_shadersPath;

    constexpr uint64_t kEditRevisionBit = 1ull << 63;   // отдельная нумерация: не пересекается с ревизиями MapMesh
    uint64_t s_revisionCounter = 0;

    static_assert(sizeof(EditGeometry::Shading) == 5 * sizeof(float), "Shading must be tightly packed for GPU upload");
}

LevelEditMesh::LevelEditMesh() = default;

LevelEditMesh::~LevelEditMesh()
{
    if (m_vao != 0)
    {
        const GLuint vao = m_vao;
        glDeleteVertexArrays(1, &vao);

        const GLuint buffers[4] = { m_positionBuffer.id, m_shadingBuffer.id, m_lightmapBuffer.id, m_indexBuffer.id };
        glDeleteBuffers(4, buffers);
    }
    if (m_whiteTexture != 0)
    {
        const GLuint texture = m_whiteTexture;
        glDeleteTextures(1, &texture);
    }
    GLState::invalidate();
}

const LevelEditMesh::TextureInfo& LevelEditMesh::texture(const std::string& name)
{
    const auto it = m_textures.find(name);
    if (it != m_textures.end()) return it->second;

    TextureInfo info;
    if (auto asset = TextureManager::Get().loadTexture("wolf/textures/" + name))
    {
        info.width = asset->getWidth();
        info.height = asset->getHeight();
        info.handle = asset->getHandle();
    }
    return m_textures.emplace(name, info).first->second;
}

uint32_t LevelEditMesh::materialSlot(const std::string& name)
{
    const auto it = m_slotByName.find(name);
    if (it != m_slotByName.end()) return it->second;

    std::string mapParam = name;
    BlendMode blendMode = BlendMode::Opaque;
    s_shaders.getBaseTextureName("textures/" + name, mapParam, blendMode);

    Material material;
    material.m_name = name;
    material.blendMode = blendMode;
    material.albedoMap = texture(name).handle;
    material.isEmbedded = false;

    const uint32_t slot = static_cast<uint32_t>(m_materials.size());
    m_materials.push_back(std::move(material));
    m_slotByName.emplace(name, slot);
    return slot;
}

EditGeometry::SizeResolver LevelEditMesh::sizeResolver()
{
    return [this](const std::string& name)
    {
        const TextureInfo& info = texture(name);
        return glm::ivec2(info.width, info.height);
    };
}

void LevelEditMesh::createGpuObjects()
{
    glGenVertexArrays(1, &m_vao);

    GLuint buffers[4] = {};
    glGenBuffers(4, buffers);
    m_positionBuffer.id = buffers[0];
    m_shadingBuffer.id = buffers[1];
    m_lightmapBuffer.id = buffers[2];
    m_indexBuffer.id = buffers[3];

    glBindVertexArray(m_vao);

    const auto attribute = [](VertexAttributeLocation location, GLint components, GLsizei stride, size_t offset)
    {
        const GLuint index = static_cast<GLuint>(location);
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, components, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offset));
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer.id);
    attribute(VertexAttributeLocation::Position, 3, sizeof(glm::vec3), 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_shadingBuffer.id);
    attribute(VertexAttributeLocation::Normal, 3, sizeof(EditGeometry::Shading), offsetof(EditGeometry::Shading, normal));
    attribute(VertexAttributeLocation::TexCoord, 2, sizeof(EditGeometry::Shading), offsetof(EditGeometry::Shading, texCoord));

    glBindBuffer(GL_ARRAY_BUFFER, m_lightmapBuffer.id);
    attribute(VertexAttributeLocation::TexCoord2, 2, sizeof(glm::vec2), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer.id);   // привязка запоминается в VAO

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    GLState::invalidate();
}

void LevelEditMesh::uploadWhiteTexture()
{
    // Лайтмапы в режиме правки нет: белая 1x1 даёт множитель 1 (как у MapMesh до запекания)
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

// Все заливки идут через GL_COPY_WRITE_BUFFER: буфер не привязывается к своим целям, и состояние VAO не страдает
size_t LevelEditMesh::uploadTail(Buffer& buffer, const void* data, size_t count, size_t elementSize, size_t first)
{
    const size_t bytes = count * elementSize;
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer.id);

    if (bytes > buffer.capacity)
    {
        buffer.capacity = std::max(bytes, buffer.capacity + buffer.capacity / 2);   // рост в 1.5 раза: реаллокации редки
        glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(buffer.capacity), nullptr, GL_DYNAMIC_DRAW);
        first = 0;                                                                   // хранилище новое: префикса в нём нет
    }

    first = std::min(first, count);
    const size_t tailBytes = (count - first) * elementSize;
    if (tailBytes > 0)
    {
        glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(first * elementSize), static_cast<GLsizeiptr>(tailBytes),
                        static_cast<const char*>(data) + first * elementSize);
    }
    return tailBytes;
}

void LevelEditMesh::uploadRange(const Buffer& buffer, size_t offsetBytes, const void* data, size_t bytes)
{
    if (bytes == 0) return;

    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer.id);
    glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(offsetBytes), static_cast<GLsizeiptr>(bytes), data);
}

void LevelEditMesh::ensureLightmapBuffer()
{
    // Нули на каждую вершину ёмкости: вершинный буфер мог вырасти
    const size_t vertexCapacity = m_positionBuffer.capacity / sizeof(glm::vec3);
    const size_t needed = vertexCapacity * sizeof(glm::vec2);
    if (needed <= m_lightmapBuffer.capacity) return;

    const std::vector<float> zeros(needed / sizeof(float), 0.0f);
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_lightmapBuffer.id);
    glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(needed), zeros.data(), GL_STATIC_DRAW);
    m_lightmapBuffer.capacity = needed;
}

void LevelEditMesh::rebuildSurfaces()
{
    m_surfaces.clear();
    m_surfaceBounds.clear();

    for (const EditGeometry::Chunk* chunk : m_geometry.chunks())
    {
        Surface surface;
        surface.m_name = "surface_" + chunk->material;
        surface.materialSlot = materialSlot(chunk->material);
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

    if (m_vao == 0) createGpuObjects();
    if (m_whiteTexture == 0) uploadWhiteTexture();

    const auto& positions = m_geometry.positions();
    const auto& shading = m_geometry.shading();
    const auto& indices = m_geometry.indices();

    size_t uploaded = 0;

    if (result.layoutChanged)
    {
        // Раскладка сдвинулась: все чанки до первого изменившегося остались на месте, льём только хвост
        uploaded += uploadTail(m_positionBuffer, positions.data(), positions.size(), sizeof(glm::vec3), result.vertexTail.start);
        uploaded += uploadTail(m_shadingBuffer, shading.data(), shading.size(), sizeof(EditGeometry::Shading), result.vertexTail.start);
        uploaded += uploadTail(m_indexBuffer, indices.data(), indices.size(), sizeof(uint32_t), result.indexTail.start);
        ensureLightmapBuffer();
    }
    else
    {
        // Раскладка прежняя: переливаем только затронутые чанки
        for (const EditGeometry::Range& range : result.vertexRanges)
        {
            uploadRange(m_positionBuffer, range.start * sizeof(glm::vec3), positions.data() + range.start, range.count * sizeof(glm::vec3));
            uploadRange(m_shadingBuffer, range.start * sizeof(EditGeometry::Shading), shading.data() + range.start, range.count * sizeof(EditGeometry::Shading));
            uploaded += range.count * (sizeof(glm::vec3) + sizeof(EditGeometry::Shading));
        }
        for (const EditGeometry::Range& range : result.indexRanges)
        {
            uploadRange(m_indexBuffer, range.start * sizeof(uint32_t), indices.data() + range.start, range.count * sizeof(uint32_t));
            uploaded += range.count * sizeof(uint32_t);
        }
    }

    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    GLState::invalidate();   // привязки изменены мимо кэша

    if (result.layoutChanged || result.boundsChanged) rebuildSurfaces();

    m_stats.vertices = positions.size();
    m_stats.indices = indices.size();
    m_stats.lastUploadBytes = uploaded;
    m_stats.lastLayoutChanged = result.layoutChanged;

    m_revision = kEditRevisionBit | ++s_revisionCounter;   // RenderScene перестроит статику (границы поверхностей могли поменяться)
}

void LevelEditMesh::build(const LevelDocument& level)
{
    const std::string scriptsPath = (level.basePath / "scripts").string();
    if (scriptsPath != s_shadersPath)
    {
        s_shaders.initFromDir(scriptsPath.c_str());
        s_shadersPath = scriptsPath;
    }

    m_textures.clear();
    m_materials.clear();
    m_slotByName.clear();

    upload(m_geometry.rebuild(level, sizeResolver()));

    std::cout << "[LevelEditMesh] Built edit geometry. Vertices: " << m_stats.vertices << ", surfaces: " << m_stats.surfaces << std::endl;
}

void LevelEditMesh::apply(const LevelDocument& level, const ChangeSet& changes)
{
    upload(m_geometry.update(level, changes, sizeResolver()));
}
