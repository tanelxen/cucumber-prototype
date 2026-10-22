//
//  RenderScene.cpp
//  Cucumber
//

#include "RenderScene.h"

#include <algorithm>

void RenderScene::beginFrame()
{
    dynamicObjects.clear();
    lights.clear();
}

void RenderScene::resetStatic(uint64_t key)
{
    staticObjects.clear();
    m_staticGrid.clear();
    m_staticKey = key;
}

void RenderScene::addStatic(const RenderObject& object)
{
    staticObjects.push_back(object);
}

void RenderScene::finalizeStatic()
{
    for (uint32_t i = 0; i < staticObjects.size(); ++i)
    {
        m_staticGrid.insert(i, staticObjects[i].bounds);
    }
}

void RenderScene::cull(const Frustum& frustum, uint32_t layerMask, CullResult& out) const
{
    out.visibleStatic.clear();
    out.visibleDynamic.clear();

    m_staticGrid.queryFrustum(frustum, m_scratch, out.visibleStatic);

    out.visibleStatic.erase(
        std::remove_if(out.visibleStatic.begin(), out.visibleStatic.end(),
                       [&](uint32_t id) { return (staticObjects[id].layer & layerMask) == 0; }),
        out.visibleStatic.end());

    // Порядок id = порядок построения (ячейка -> материал): одинаковые материалы идут рядом
    std::sort(out.visibleStatic.begin(), out.visibleStatic.end());

    for (uint32_t i = 0; i < dynamicObjects.size(); ++i)
    {
        const RenderObject& object = dynamicObjects[i];
        if ((object.layer & layerMask) == 0) continue;
        if (object.bounds.valid() && !frustum.intersects(object.bounds)) continue;

        out.visibleDynamic.push_back(i);
    }
}

void RenderScene::queryAABB(const AABB& box, uint32_t layerMask, CullResult& out) const
{
    out.visibleStatic.clear();
    out.visibleDynamic.clear();

    m_staticGrid.queryAABB(box, m_scratch, out.visibleStatic);

    out.visibleStatic.erase(
        std::remove_if(out.visibleStatic.begin(), out.visibleStatic.end(),
                       [&](uint32_t id) { return (staticObjects[id].layer & layerMask) == 0; }),
        out.visibleStatic.end());

    std::sort(out.visibleStatic.begin(), out.visibleStatic.end());

    for (uint32_t i = 0; i < dynamicObjects.size(); ++i)
    {
        const RenderObject& object = dynamicObjects[i];
        if ((object.layer & layerMask) == 0) continue;
        if (object.bounds.valid() && !box.intersects(object.bounds)) continue;

        out.visibleDynamic.push_back(i);
    }
}
