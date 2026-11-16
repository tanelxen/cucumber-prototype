//
//  GpuBuffer.h
//  Cucumber
//
//  Общая основа VertexBuffer и IndexBuffer: владение GL-объектом, ёмкость, рост и частичная заливка.
//
//  Все операции идут через GL_COPY_WRITE_BUFFER: буфер не привязывается к GL_ARRAY_BUFFER /
//  GL_ELEMENT_ARRAY_BUFFER, поэтому привязка индексного буфера в текущем VAO не затрагивается
//  (GL_ELEMENT_ARRAY_BUFFER является состоянием VAO, и случайный bind под чужим VAO подменяет ему индексы).
//
//  VAO хранит id буфера, а не его хранилище, поэтому рост через glBufferData VAO не ломает.
//

#pragma once

#include <cstddef>
#include <cstdint>

enum class BufferUsage : uint8_t
{
    Static,    // залили и рисуем
    Dynamic    // обновляется частями
};

class GpuBuffer
{
public:
    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    uint32_t getID() const { return m_id; }
    size_t capacity() const { return m_capacity; }   // байт

    // Гарантирует ёмкость не меньше bytes. Растёт в 1.5 раза, поэтому реаллокации редки.
    // Возвращает true, если хранилище пересоздано: прежнее содержимое потеряно, а новое не определено.
    bool reserve(size_t bytes);

    // Перезаписывает диапазон. Выход за ёмкость не проверяется.
    void write(size_t offsetBytes, const void* data, size_t bytes);

    // Заливает элементы [first, count) массива (хвост). Если буфер пришлось увеличить, префикса в нём нет,
    // и заливается всё с нуля. Возвращает число залитых байт.
    size_t writeTail(const void* data, size_t count, size_t elementSize, size_t first);

protected:
    explicit GpuBuffer(BufferUsage usage);
    ~GpuBuffer();

    // Заменяет хранилище целиком (glBufferData)
    void allocate(const void* data, size_t bytes);

private:
    uint32_t m_id = 0;
    size_t m_capacity = 0;
    BufferUsage m_usage;
};
