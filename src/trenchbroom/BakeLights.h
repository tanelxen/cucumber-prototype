//
//  BakeLights.h
//  Cucumber
//
//  Источники света сцены, которые участвуют в запекании лайтмапы. Раньше жило в MapComponent,
//  хотя к карте отношения не имеет: нужна только сцена.
//

#pragma once

#include "LightmapBaker.h"

#include <vector>

class WorldScene;

// Активные включённые лампы с режимом Baked или BakedAndRealtime
std::vector<LightmapBakeLight> CollectBakeLights(const WorldScene& scene);
