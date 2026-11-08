//
//  LevelWireframe.h
//  Cucumber
//
//  Каркас солидов уровня для видов в режиме Wireframe (ортографические проекции, как в TrenchBroom / Hammer).
//  Рёбра всех солидов документа собираются в ПОСТОЯННЫЙ набор линий OverlayRender (GPU-буфер) и перестраиваются
//  только при смене ревизии карты (правка, undo / redo, загрузка), а не каждый кадр.
//
//  Редакторский код: в игре (standalone) не используется. Источник данных LevelDocument (MapComponent::level),
//  поэтому каркас работает и вне режима правки.
//
//  Использование: UIManager владеет экземпляром и раз в кадр, если есть панель в режиме Wireframe, вызывает sync();
//  вернувшийся id уходит в ViewDesc::overlayLines нужных видов, а RenderDevice рисует набор под оверлеем правки.
//

#pragma once

#include "renderer/OverlayRender.h"

#include <cstdint>
#include <vector>

struct LevelDocument;
struct MapComponent;

class LevelWireframe
{
public:
    LevelWireframe() = default;
    ~LevelWireframe();

    LevelWireframe(const LevelWireframe&) = delete;
    LevelWireframe& operator=(const LevelWireframe&) = delete;

    // Приводит набор линий в соответствие с картой (перестройка только при смене ревизии).
    // Возвращает id набора для ViewDesc::overlayLines, 0 если рисовать нечего (нет карты).
    uint32_t sync(MapComponent* map);

    // Освобождает набор линий (GL-буфер)
    void release();

    // Рёбра всех солидов документа, каждое ровно один раз. Без GL.
    static void Collect(const LevelDocument& level, std::vector<OverlayRender::StaticLine>& out);

private:
    uint32_t m_set = 0;

    const LevelDocument* m_document = nullptr;
    uint64_t m_revision = 0;
    bool m_built = false;
};
