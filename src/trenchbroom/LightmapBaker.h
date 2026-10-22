//
//  LightmapBaker.h
//  Cucumber
//
//  Запекание прямого освещения в лайтмапу через NanoRT.
//
//  v2: многопоточность (каждый поток со своим intersector'ом), дилатация границ чартов,
//      результат в RGB16F без клиппинга (раньше RGB8 с делением на 10), отмена и прогресс.
//      Ambient из бейка убран: его добавляет шейдер (vec3(0.08)), иначе он учитывался дважды.
//

#pragma once

#include "Mesh.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// Меняйте при любом изменении алгоритма: кэш лайтмап на диске завязан на это число.
constexpr uint32_t kLightmapBakerVersion = 2;

enum class LightmapBakeLightType
{
    Directional,
    Point,
    Spot
};

struct LightmapBakeLight
{
    LightmapBakeLightType type = LightmapBakeLightType::Point;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float spotAngle = 30.0f;
};

struct LightmapBakeSettings
{
    int dilationPasses = 2;      // на сколько текселей растягиваем край чарта в padding (<= padding атласа)
    float rayBias = 0.5f;        // смещение начала теневого луча вдоль нормали (юниты)
    float ambient = 0.0f;        // постоянная добавка к освещённости (шейдер уже добавляет свою)
    unsigned maxThreads = 0;     // 0 = все ядра
    
    int giBounces = 2;
    int giSamples = 32;
};

// Всё, что нужно бейку. Копия данных: задача может жить дольше меша и бежать в другом потоке.
struct LightmapBakeInput
{
    std::vector<VertexPosition> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
    std::vector<glm::vec2> lightmapTexCoords;
    int atlasSize = 0;
    std::vector<LightmapBakeLight> lights;
    LightmapBakeSettings settings;
};

struct LightmapBakeResult
{
    std::vector<uint16_t> pixels;   // RGB, half float, строки сверху вниз как в атласе; width * height * 3
    int width = 0;
    int height = 0;
};

struct LightmapBakeControl
{
    std::atomic<float> progress{0.0f};
    std::atomic<bool> cancel{false};
};

class LightmapBaker
{
public:
    // Блокирующий. Можно звать из любого потока; control (необязателен) читается/пишется атомарно.
    static bool bake(const LightmapBakeInput& input,
                     LightmapBakeResult& result,
                     std::string& error,
                     LightmapBakeControl* control = nullptr);
};
