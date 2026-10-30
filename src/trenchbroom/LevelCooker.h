//
//  LevelCooker.h
//  Cucumber
//
//  LevelDocument -> данные для рендера (MapMeshBuildResult) и физики. Детерминирован: один и тот же документ
//  всегда даёт один и тот же результат, включая раскладку лайтмапы (от этого зависит кэш лайтмапы).
//
//  Заменяет MapMeshBuilder::build(MapAsset): разбор плоскостей теперь в импортёре, здесь только нарезка,
//  UV, чарты лайтмапы и упаковка атласа. Результат того же типа, поэтому MapMesh (GPU-часть) не меняется.
//

#pragma once

#include "level/LevelDocument.h"
#include "MapMeshBuilder.h"   // LightmapUnwrapSettings, MapMeshBuildResult, MapMeshBuilder::TextureDimensionsResolver

#include <string>
#include <vector>

#include <glm/glm.hpp>

class LevelCooker
{
public:
    static bool cook(const LevelDocument& level,
                     const LightmapUnwrapSettings& unwrapSettings,
                     const MapMeshBuilder::TextureDimensionsResolver& getTextureDimensions,
                     MapMeshBuildResult& result,
                     std::string& error);

    // Вершины каждого солида (пространство движка): из них строятся выпуклые оболочки физики.
    // Включают грани NoDraw: clip-браши обязаны быть твёрдыми.
    static void collisionHulls(const LevelDocument& level, std::vector<std::vector<glm::vec3>>& out);
};
