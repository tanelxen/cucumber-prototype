//
//  LightmapBaker.cpp
//  Cucumber
//

#include "LightmapBaker.h"

#include "Half.h"
#include "Parallel.h"

#include <nanort.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    struct Texel
    {
        glm::vec3 position;
        glm::vec3 normal;
        uint32_t pixel;   // y * atlasSize + x
    };

    // Простой и быстрый потокобезопасный генератор случайных чисел (Xorshift)
    float RandomFloat(uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state) / 4294967296.0f;
    }

    // Генерация случайного направления в полусфере с косинусным распределением
    glm::vec3 GenerateCosineWeightedDirection(const glm::vec3& normal, uint32_t& randState)
    {
        float u1 = RandomFloat(randState);
        float u2 = RandomFloat(randState);

        float r = std::sqrt(u1);
        float theta = 6.283185307f * u2;

        float x = r * std::cos(theta);
        float y = r * std::sin(theta);
        float z = std::sqrt(std::max(0.0f, 1.0f - u1));

        // Строим ортонормированный базис вокруг нормали
        glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
        glm::vec3 bitangent = glm::cross(normal, tangent);

        return tangent * x + bitangent * y + normal * z;
    }

    bool IsCancelled(const LightmapBakeControl* control)
    {
        return control && control->cancel.load(std::memory_order_relaxed);
    }

    void SetProgress(LightmapBakeControl* control, float value)
    {
        if (control) control->progress.store(value, std::memory_order_relaxed);
    }

    // Выборка цвета из лайтмапы предыдущего шага по UV-координатам
    glm::vec3 SamplePreviousLightmap(const std::vector<float>& previousImage, int atlasSize, const glm::vec2& uv)
    {
        int x = glm::clamp(static_cast<int>(std::floor(uv.x * static_cast<float>(atlasSize))), 0, atlasSize - 1);
        int y = glm::clamp(static_cast<int>(std::floor(uv.y * static_cast<float>(atlasSize))), 0, atlasSize - 1);
        size_t idx = (static_cast<size_t>(y) * atlasSize + x) * 3;
        return glm::vec3(previousImage[idx + 0], previousImage[idx + 1], previousImage[idx + 2]);
    }

    // Освещённость одного текселя (Прямой свет + Вторичные лучи GI)
    glm::vec3 ShadeTexel(const Texel& texel,
                         const LightmapBakeInput& input,
                         const nanort::BVHAccel<float>& acceleration,
                         const nanort::TriangleIntersector<>& intersector,
                         const nanort::BVHTraceOptions& traceOptions,
                         const std::vector<float>& previousPassImage,
                         int currentBounce,
                         uint32_t& randState)
    {
        const auto isVisible = [&](const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
        {
            nanort::Ray<float> ray;
            ray.org[0] = origin.x;    ray.org[1] = origin.y;    ray.org[2] = origin.z;
            ray.dir[0] = direction.x; ray.dir[1] = direction.y; ray.dir[2] = direction.z;
            ray.min_t = 0.001f;
            ray.max_t = maxDistance;

            nanort::TriangleIntersection<float> intersection;
            return !acceleration.Traverse(ray, intersector, &intersection, traceOptions);
        };

        glm::vec3 directIrradiance(0.0f);

        // Расчёт ПРЯМОГО освещения выполняется только на первом пассе (Пасс 0)
        if (currentBounce == 0)
        {
            directIrradiance = glm::vec3(input.settings.ambient);

            for (const auto& light : input.lights)
            {
                glm::vec3 directionToLight;
                float maxDistance = 8192.0f;
                float attenuation = 1.0f;

                if (light.type == LightmapBakeLightType::Directional)
                {
                    const float directionLength = glm::length(light.direction);
                    if (directionLength < 1e-5f) continue;
                    directionToLight = -light.direction / directionLength;
                }
                else
                {
                    const glm::vec3 toLight = light.position - texel.position;
                    const float distance = glm::length(toLight);
                    if (distance < 1e-3f || light.range <= 0.0f || distance >= light.range) continue;

                    directionToLight = toLight / distance;
                    maxDistance = distance - input.settings.rayBias;

                    const float ratio = distance / light.range;
                    const float ratio2 = ratio * ratio;
                    const float ratio4 = ratio2 * ratio2;
                    attenuation = (1.0f - ratio4) * (1.0f - ratio4);

                    if (light.type == LightmapBakeLightType::Spot)
                    {
                        const float directionLength = glm::length(light.direction);
                        if (directionLength < 1e-5f) continue;

                        const float coneCosine = glm::dot(-directionToLight, light.direction / directionLength);
                        const float outerCosine = std::cos(glm::radians(light.spotAngle));
                        const float innerCosine = std::cos(glm::radians(light.spotAngle * 0.8f));
                        attenuation *= glm::clamp((coneCosine - outerCosine) / std::max(innerCosine - outerCosine, 1e-5f),
                                                  0.0f, 1.0f);
                    }
                }

                const float diffuse = std::max(glm::dot(texel.normal, directionToLight), 0.0f);
                if (diffuse <= 0.0f || attenuation <= 0.0f) continue;
                if (!isVisible(texel.position + texel.normal * input.settings.rayBias, directionToLight, maxDistance)) continue;

                directIrradiance += light.color * (light.intensity * attenuation * diffuse);
            }
        }

        // Расчёт НЕПРЯМОГО освещения (Global Illumination переотражения)
        glm::vec3 indirectIrradiance(0.0f);
        if (currentBounce > 0 && input.settings.giSamples > 0)
        {
            for (int s = 0; s < input.settings.giSamples; ++s)
            {
                glm::vec3 rayDir = GenerateCosineWeightedDirection(texel.normal, randState);

                nanort::Ray<float> indirectRay;
                indirectRay.org[0] = texel.position.x + texel.normal.x * input.settings.rayBias;
                indirectRay.org[1] = texel.position.y + texel.normal.y * input.settings.rayBias;
                indirectRay.org[2] = texel.position.z + texel.normal.z * input.settings.rayBias;
                indirectRay.dir[0] = rayDir.x;
                indirectRay.dir[1] = rayDir.y;
                indirectRay.dir[2] = rayDir.z;
                indirectRay.min_t = 0.001f;
                indirectRay.max_t = 8192.0f;

                nanort::TriangleIntersection<float> hit;
                if (acceleration.Traverse(indirectRay, intersector, &hit))
                {
                    uint32_t i0 = input.indices[hit.prim_id * 3 + 0];
                    uint32_t i1 = input.indices[hit.prim_id * 3 + 1];
                    uint32_t i2 = input.indices[hit.prim_id * 3 + 2];

                    // Интерполируем Lightmap UV в точке попадания вторичного луча
                    float w1 = hit.u;
                    float w2 = hit.v;
                    float w0 = 1.0f - w1 - w2;
                    glm::vec2 hitUV = input.lightmapTexCoords[i0] * w0 +
                                      input.lightmapTexCoords[i1] * w1 +
                                      input.lightmapTexCoords[i2] * w2;

                    // Читаем накопленный свет из предыдущей итерации запекания
                    glm::vec3 hitColor = SamplePreviousLightmap(previousPassImage, input.atlasSize, hitUV);
                    
                    // Так как цвет текстуры пока игнорируется, считаем альбедо материала базовым серым (0.5)
                    // Для физичности Lambertian GI умножаем на 1.0 (косинусное распределение уже скомпенсировано в GenerateCosineWeightedDirection)
                    indirectIrradiance += hitColor * 0.25f; 
                }
            }
            indirectIrradiance /= static_cast<float>(input.settings.giSamples);
        }

        return directIrradiance + indirectIrradiance;
    }

    void Dilate(std::vector<float>& rgb, std::vector<uint8_t>& coverage, int size, int passes, unsigned maxThreads)
    {
        std::vector<float> nextRgb;
        std::vector<uint8_t> nextCoverage;

        for (int pass = 0; pass < passes; ++pass)
        {
            nextRgb = rgb;
            nextCoverage = coverage;

            ParallelForChunks(static_cast<size_t>(size), 16, maxThreads, [&](size_t y0, size_t y1)
            {
                for (size_t y = y0; y < y1; ++y)
                {
                    for (int x = 0; x < size; ++x)
                    {
                        const size_t index = y * static_cast<size_t>(size) + static_cast<size_t>(x);
                        if (coverage[index]) continue;

                        glm::vec3 sum(0.0f);
                        int count = 0;
                        for (int dy = -1; dy <= 1; ++dy)
                        {
                            for (int dx = -1; dx <= 1; ++dx)
                            {
                                if (dx == 0 && dy == 0) continue;

                                const int nx = x + dx;
                                const int ny = static_cast<int>(y) + dy;
                                if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;

                                const size_t neighbor = static_cast<size_t>(ny) * static_cast<size_t>(size)
                                                      + static_cast<size_t>(nx);
                                if (!coverage[neighbor]) continue;

                                sum += glm::vec3(rgb[neighbor * 3 + 0], rgb[neighbor * 3 + 1], rgb[neighbor * 3 + 2]);
                                ++count;
                            }
                        }

                        if (count > 0)
                        {
                            sum /= static_cast<float>(count);
                            nextRgb[index * 3 + 0] = sum.x;
                            nextRgb[index * 3 + 1] = sum.y;
                            nextRgb[index * 3 + 2] = sum.z;
                            nextCoverage[index] = 1;
                        }
                    }
                }
            });

            rgb.swap(nextRgb);
            coverage.swap(nextCoverage);
        }
    }
}

bool LightmapBaker::bake(const LightmapBakeInput& input,
                         LightmapBakeResult& result,
                         std::string& error,
                         LightmapBakeControl* control)
{
    result = {};
    SetProgress(control, 0.0f);

    const int atlasSize = input.atlasSize;
    if (atlasSize <= 0 || input.positions.empty() || input.indices.empty()
        || input.positions.size() != input.normals.size()
        || input.positions.size() != input.lightmapTexCoords.size())
    {
        error = "Map mesh has no valid lightmap unwrap";
        return false;
    }
    if (input.lights.empty())
    {
        error = "No enabled baked light sources in the scene";
        return false;
    }
    if (input.indices.size() % 3 != 0)
    {
        error = "Map mesh index count is not a multiple of three";
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(atlasSize) * static_cast<size_t>(atlasSize);
    
    // ---- 1. Растеризация UV с субпиксельной сеткой (2х2 MSAA / SuperSampling) ----
    std::vector<Texel> texels;
    std::vector<uint8_t> baseCoverage(pixelCount, 0);

    const float atlasSizeF = static_cast<float>(atlasSize);
    const int subSamplesX = 2;
    const int subSamplesY = 2;
    const float stepX = 1.0f / static_cast<float>(subSamplesX);
    const float stepY = 1.0f / static_cast<float>(subSamplesY);
    
    for (size_t triangle = 0; triangle < input.indices.size(); triangle += 3)
    {
        const uint32_t i0 = input.indices[triangle + 0];
        const uint32_t i1 = input.indices[triangle + 1];
        const uint32_t i2 = input.indices[triangle + 2];
        if (i0 >= input.positions.size() || i1 >= input.positions.size() || i2 >= input.positions.size())
        {
            error = "Map mesh contains an invalid triangle index";
            result = {};
            return false;
        }

        const glm::vec2 uv0 = input.lightmapTexCoords[i0] * atlasSizeF;
        const glm::vec2 uv1 = input.lightmapTexCoords[i1] * atlasSizeF;
        const glm::vec2 uv2 = input.lightmapTexCoords[i2] * atlasSizeF;
        const float area = (uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv1.y - uv0.y) * (uv2.x - uv0.x);
        if (std::abs(area) < 1e-6f) continue;

        const int minX = std::max(0, static_cast<int>(std::floor(std::min({uv0.x, uv1.x, uv2.x}))));
        const int maxX = std::min(atlasSize - 1, static_cast<int>(std::ceil(std::max({uv0.x, uv1.x, uv2.x}))));
        const int minY = std::max(0, static_cast<int>(std::floor(std::min({uv0.y, uv1.y, uv2.y}))));
        const int maxY = std::min(atlasSize - 1, static_cast<int>(std::ceil(std::max({uv0.y, uv1.y, uv2.y}))));

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const size_t pixel = static_cast<size_t>(y) * static_cast<size_t>(atlasSize) + static_cast<size_t>(x);
                if (baseCoverage[pixel]) continue; 

                bool pixelCoveredByThisTriangle = false;
                
                // Цикл генерации субпикселей внутри одного текселя
                for (int sy = 0; sy < subSamplesY; ++sy)
                {
                    for (int sx = 0; sx < subSamplesX; ++sx)
                    {
                        const glm::vec2 point(
                            static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) * stepX,
                            static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) * stepY
                        );

                        const float w1 = ((point.x - uv0.x) * (uv2.y - uv0.y) - (point.y - uv0.y) * (uv2.x - uv0.x)) / area;
                        const float w2 = ((uv1.x - uv0.x) * (point.y - uv0.y) - (uv1.y - uv0.y) * (point.x - uv0.x)) / area;
                        const float w0 = 1.0f - w1 - w2;
                        if (w0 < -0.001f || w1 < -0.001f || w2 < -0.001f) continue;

                        Texel texel;
                        texel.position = input.positions[i0].position * w0 + input.positions[i1].position * w1 + input.positions[i2].position * w2;
                        texel.normal = glm::normalize(input.normals[i0] * w0 + input.normals[i1] * w1 + input.normals[i2] * w2);
                        texel.pixel = static_cast<uint32_t>(pixel);
                        texels.push_back(texel);
                        pixelCoveredByThisTriangle = true;
                    }
                }
                if (pixelCoveredByThisTriangle)
                {
                    baseCoverage[pixel] = 1;
                }
            }
        }

        if ((triangle / 3) % 4096 == 0)
        {
            if (IsCancelled(control)) { error = "Cancelled"; result = {}; return false; }
            SetProgress(control, 0.05f * static_cast<float>(triangle) / static_cast<float>(input.indices.size()));
        }
    }

    if (texels.empty())
    {
        error = "Lightmap UVs cover no texels";
        return false;
    }
    SetProgress(control, 0.05f);

    // ---- 2. BVH ----
    std::vector<float> vertices;
    vertices.reserve(input.positions.size() * 3);
    for (const auto& vertex : input.positions)
    {
        vertices.push_back(vertex.position.x);
        vertices.push_back(vertex.position.y);
        vertices.push_back(vertex.position.z);
    }

    nanort::TriangleMesh<float> triangleMesh(vertices.data(), input.indices.data(), sizeof(float) * 3);
    nanort::TriangleSAHPred<float> trianglePredicate(vertices.data(), input.indices.data(), sizeof(float) * 3);
    nanort::BVHBuildOptions<float> buildOptions;
    nanort::BVHAccel<float> acceleration;
    if (!acceleration.Build(static_cast<unsigned int>(input.indices.size() / 3),
                            triangleMesh, trianglePredicate, buildOptions))
    {
        error = "NanoRT failed to build the map BVH";
        return false;
    }
    if (IsCancelled(control)) { error = "Cancelled"; return false; }
    SetProgress(control, 0.05f);

    // ---- 3. Итеративное многопроходное запекание (Прямой свет + Пассы GI) ----
    std::vector<float> accumulatedImage(pixelCount * 3, 0.0f);
    std::vector<float> currentPassImage(pixelCount * 3, 0.0f);
    
    int totalBounces = std::max(0, input.settings.giBounces);
    float progressStep = 0.90f / static_cast<float>(totalBounces + 1);

    for (int bounce = 0; bounce <= totalBounces; ++bounce)
    {
        std::vector<glm::vec3> texelColor(texels.size(), glm::vec3(0.0f));
        std::atomic<size_t> processed{0};
        std::atomic<bool> aborted{false};

        ParallelForChunks(texels.size(), 512, input.settings.maxThreads, [&](size_t begin, size_t end)
        {
            if (aborted.load(std::memory_order_relaxed)) return;
            if (IsCancelled(control)) { aborted.store(true); return; }

            const nanort::TriangleIntersector<> intersector(vertices.data(), input.indices.data(), sizeof(float) * 3);
            const nanort::BVHTraceOptions traceOptions;
            
            // Навешиваем уникальный seed для генератора случайных чисел потока на основе индекса
            uint32_t randState = 1337 + static_cast<uint32_t>(begin) * 73 + static_cast<uint32_t>(bounce) * 101;

            for (size_t i = begin; i < end; ++i)
            {
                texelColor[i] = ShadeTexel(texels[i], input, acceleration, intersector, traceOptions, currentPassImage, bounce, randState);
            }

            const size_t done = processed.fetch_add(end - begin) + (end - begin);
            float passProgress = static_cast<float>(done) / static_cast<float>(texels.size());
            SetProgress(control, 0.05f + progressStep * (static_cast<float>(bounce) + passProgress));
        });

        if (aborted.load() || IsCancelled(control))
        {
            error = "Cancelled";
            return false;
        }

        // Сборка текущего пасса
        std::fill(currentPassImage.begin(), currentPassImage.end(), 0.0f);
        std::vector<int> subSampleCount(pixelCount, 0);

        for (size_t i = 0; i < texels.size(); ++i)
        {
            const size_t pixel = texels[i].pixel;
            currentPassImage[pixel * 3 + 0] += texelColor[i].x;
            currentPassImage[pixel * 3 + 1] += texelColor[i].y;
            currentPassImage[pixel * 3 + 2] += texelColor[i].z;
            subSampleCount[pixel]++;
        }

        for (size_t pixel = 0; pixel < pixelCount; ++pixel)
        {
            if (subSampleCount[pixel] > 0)
            {
                const float invCount = 1.0f / static_cast<float>(subSampleCount[pixel]);
                currentPassImage[pixel * 3 + 0] *= invCount;
                currentPassImage[pixel * 3 + 1] *= invCount;
                currentPassImage[pixel * 3 + 2] *= invCount;
            }
            
            // Суммируем вклад текущего пасса в общую финальную карту
            accumulatedImage[pixel * 3 + 0] += currentPassImage[pixel * 3 + 0];
            accumulatedImage[pixel * 3 + 1] += currentPassImage[pixel * 3 + 1];
            accumulatedImage[pixel * 3 + 2] += currentPassImage[pixel * 3 + 2];
        }
    }

    // ---- 4. Дилатация финального накопленного результата ----
    if (input.settings.dilationPasses > 0)
    {
        Dilate(accumulatedImage, baseCoverage, atlasSize, input.settings.dilationPasses, input.settings.maxThreads);
    }
    SetProgress(control, 0.95f);

    // ---- 5. RGB16F ----
    result.width = atlasSize;
    result.height = atlasSize;
    result.pixels.resize(accumulatedImage.size());
    ParallelForChunks(accumulatedImage.size(), 1 << 16, input.settings.maxThreads, [&](size_t begin, size_t end)
    {
        for (size_t i = begin; i < end; ++i) result.pixels[i] = FloatToHalf(accumulatedImage[i]);
    });

    SetProgress(control, 1.0f);
    error.clear();
    return true;
}
