//
//  Q3Lightmapper.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 14.09.26.
//

#include "Q3Lightmapper.h"

// Структура для промежуточного хранения размеров островов при перепаковке
struct Rect {
    int id;
    int w, h;
    int x, y;
};

bool ComputeBarycentric2D(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, glm::vec3& outW)
{
    glm::vec2 v0 = b - a, v1 = c - a, v2 = p - a;
    float den = v0.x * v1.y - v1.x * v0.y;
    if (std::abs(den) < 0.00001f) return false;
    
    float v = (v2.x * v1.y - v1.x * v2.y) / den;
    float w = (v0.x * v2.y - v2.x * v0.y) / den;
    float u = 1.0f - v - w;
    
    outW = glm::vec3(u, v, w);
    const float eps = -0.001f;
    return (u >= eps && v >= eps && w >= eps);
}

bool PackRects(std::vector<Rect>& rects, int atlasWidth, int atlasHeight, int padding)
{
    // Сортируем прямоугольники по высоте для более плотной упаковки
    std::sort(rects.begin(), rects.end(), [](const Rect& a, const Rect& b) {
        return a.h > b.h;
    });
    
    int shelfX = padding;
    int shelfY = padding;
    int currentShelfHeight = 0;
    
    for (auto& r : rects) {
        int neededW = r.w + padding;
        int neededH = r.h + padding;
        
        // Если не влезаем по ширине текущей полосы, переходим на следующую строку
        if (shelfX + neededW > atlasWidth) {
            shelfX = padding;
            shelfY += currentShelfHeight;
            currentShelfHeight = 0;
        }
        
        // Если не влезаем по высоте атласа — упаковка не удалась (нужно увеличить разрешение атласа)
        if (shelfY + neededH > atlasHeight) {
            return false;
        }
        
        r.x = shelfX;
        r.y = shelfY;
        
        shelfX += neededW;
        currentShelfHeight = std::max(currentShelfHeight, neededH);
    }
    return true;
}

bool IntersectRayBrush(const Q3BSPAsset* bsp, const tBSPBrush& brush, const glm::vec3& start, const glm::vec3& end)
{
    float tMin = 0.0f;
    float tMax = 1.0f;
    
    for (int i = 0; i < brush.numOfBrushSides; ++i) {
        int sideIndex = brush.brushSide + i;
        const tBSPBrushSide& side = bsp->m_brushSides[sideIndex];
        const tBSPPlane& plane = bsp->m_planes[side.plane];
        
        float d1 = glm::dot(plane.normal, start) - plane.distance;
        float d2 = glm::dot(plane.normal, end) - plane.distance;
        
        // Если обе точки за плоскостью брэша (снаружи), луч точно его не пересекает
        if (d1 > 0.0f && d2 > 0.0f) return false;
        // Если обе точки внутри, эта плоскость нас пока не ограничивает
        if (d1 <= 0.0f && d2 <= 0.0f) continue;
        
        // Находим точку пересечения с плоскостью стороны брэша
        float t = d1 / (d1 - d2);
        if (d1 > 0.0f) {
            // Луч входит в брэш
            if (t > tMin) tMin = t;
        } else {
            // Луч выходит из брэша
            if (t < tMax) tMax = t;
        }
        
        // Если точка входа оказалась дальше точки выхода, пересечения нет
        if (tMin > tMax) return false;
    }
    
    // Если мы дошли сюда и отрезок пересечения валиден, луч застрял в геометрии брэша
    return (tMin < tMax);
}

bool TraceRayNode(const Q3BSPAsset* bsp, int nodeIndex, const glm::vec3& start, const glm::vec3& end)
{
    // МЫ ДОШЛИ ДО ЛИСТА
    if (nodeIndex < 0)
    {
        int leafIndex = -nodeIndex - 1;
        const tBSPLeaf& leaf = bsp->m_leafs[leafIndex];
        
        // Проверяем луч только со специфичными брэшами, находящимися в этом листе
        for (int i = 0; i < leaf.numOfLeafBrushes; ++i)
        {
            int brushIndex = bsp->m_leafBrushes[leaf.leafBrush + i];
            const tBSPBrush& brush = bsp->m_brushes[brushIndex];
            
            int contents = bsp->m_textures[brush.textureID].contents;
            int flags = bsp->m_textures[brush.textureID].flags;
            
            bool isSolid = contents & 1;
            bool isSky = flags & 0x4;
            
            if (isSky) return true;
            
            // Пропускаем триггеры, воду или туман (проверяем контент-флаги, если они распарсены)
            // Для базового теста: если луч пересек твердый брэш, свет заблокирован
            if (isSolid && IntersectRayBrush(bsp, brush, start, end)) {
                return false; // СТОЛКНОВЕНИЕ: Луч в тени
            }
        }
        return true; // В этом листе препятствий нет, летим дальше
    }
    
    // МЫ В УЗЛЕ ДЕРЕВА
    const tBSPNode& node = bsp->m_nodes[nodeIndex];
    const tBSPPlane& plane = bsp->m_planes[node.plane];
    
    float d1 = glm::dot(plane.normal, start) - plane.distance;
    float d2 = glm::dot(plane.normal, end) - plane.distance;
    
    // Шаг 1: Если весь луч с фронтальной стороны плоскости (>= 0)
    if (d1 >= 0.0f && d2 >= 0.0f) {
        return TraceRayNode(bsp, node.child[0], start, end);
    }
    // Шаг 2: Если весь луч с тыловой стороны плоскости (< 0)
    if (d1 < 0.0f && d2 < 0.0f) {
        return TraceRayNode(bsp, node.child[1], start, end);
    }
    
    // Шаг 3: Луч пересекает плоскость узла, нужно проверить оба дочерних поддерева.
    // Определяем, какое поддерево ближе к старту луча (куда сначала влетает луч)
    int firstChild = (d1 < 0.0f) ? 1 : 0;
    
    // Сначала проверяем то поддерево, где находится старт луча
    if (!TraceRayNode(bsp, node.child[firstChild], start, end)) {
        return false; // Наткнулись на тень в первом же поддереве
    }
    
    // Если первая половина пути чиста, проверяем вторую половину пути в соседнем поддереве
    return TraceRayNode(bsp, node.child[1 - firstChild], start, end);
}

bool TraceRay(const Q3BSPAsset* bsp, const glm::vec3& start, const glm::vec3& end) {
    return TraceRayNode(bsp, 0, start, end);
}

namespace Q3Lightmapper {
    
    void BakeCustomLightmaps(Q3BSPAsset* bsp,
                             const std::vector<LightSource>& lights,
                             TextureAtlas& outAtlas,
                             float texelsPerUnit,
                             int padding,
                             int targetAtlasSize)
    {
        if (bsp->m_faces.empty()) return;
        
        std::vector<Rect> islands;
        islands.reserve(bsp->m_faces.size());
        
        // --- ШАГ 1: Расчет физических размеров островов для каждой грани ---
        for (size_t f = 0; f < bsp->m_faces.size(); ++f)
        {
            const tBSPFace& face = bsp->m_faces[f];
            if (face.type == 3 /* FACE_FX */ || face.lightmapID == -1) continue;
            
            // Находим 3D границы (AABB) текущей грани
            glm::vec3 minBounds(100000.0f), maxBounds(-100000.0f);
            for (int i = 0; i < face.numOfVerts; ++i) {
                const auto& pos = bsp->m_verts[face.startVertIndex + i].vPosition;
                minBounds = glm::min(minBounds, pos);
                maxBounds = glm::max(maxBounds, pos);
            }
            
            glm::vec3 size = maxBounds - minBounds;
            
            // Проецируем 3D размеры на 2D плоскость в зависимости от ориентации нормали грани
            float worldW = 0.0f, worldH = 0.0f;
            glm::vec3 absNormal = glm::abs(face.vNormal);
            
            if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
                worldW = size.y; worldH = size.z; // Стена вдоль YZ
            } else if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {
                worldW = size.x; worldH = size.z; // Стена вдоль XZ
            } else {
                worldW = size.x; worldH = size.y; // Пол / Потолок вдоль XY
            }
            
            // Вычисляем необходимый размер острова в пикселях на основе плотности
            Rect r;
            r.id = static_cast<int>(f);
            r.w = std::max(2, static_cast<int>(std::ceil(worldW * texelsPerUnit)));
            r.h = std::max(2, static_cast<int>(std::ceil(worldH * texelsPerUnit)));
            islands.push_back(r);
        }
        
        // --- ШАГ 2: Упаковка всех островов в новый единый атлас ---
        outAtlas.width = targetAtlasSize;
        outAtlas.height = targetAtlasSize;
        size_t bufferSize = outAtlas.width * outAtlas.height * 3;
        outAtlas.buffer = new unsigned char[bufferSize];
        std::fill(outAtlas.buffer, outAtlas.buffer + bufferSize, 0); // Очищаем атлас
        outAtlas.tiles.clear();
        
        if (!PackRects(islands, outAtlas.width, outAtlas.height, padding)) {
            // Если не влезло, можно автоматически увеличить targetAtlasSize, но пока выведем предупреждение
            printf("Q3Lightmapper: Warning! Острова не поместились в атлас %dx%d. Увеличьте размер.\n", targetAtlasSize, targetAtlasSize);
        }
        
        // Создаем карту быстрого доступа к результатам упаковки по ID грани
        std::map<int, Rect> packedMap;
        for (const auto& r : islands) {
            packedMap[r.id] = r;
        }
        
        // --- ШАГ 3: Генерация новых UV-координат вершин и барицентрическое запекание ---
        std::vector<bool> processedVertices(bsp->m_verts.size(), false);
        
        for (size_t f = 0; f < bsp->m_faces.size(); ++f)
        {
            tBSPFace& face = bsp->m_faces[f];
            if (face.type == 3 /* FACE_FX */ || face.lightmapID == -1) continue;
            if (packedMap.find(f) == packedMap.end()) continue;
            
            const Rect& rect = packedMap[f];
            glm::vec3 texelNormal = glm::normalize(face.vNormal);
            glm::vec3 absNormal = glm::abs(texelNormal);
            
            // Сначала находим 3D-минимум для этой грани, чтобы базировать проекцию от нуля
            glm::vec3 faceMinBounds(100000.0f);
            for (int v = 0; v < face.numOfVerts; ++v) {
                faceMinBounds = glm::min(faceMinBounds, bsp->m_verts[face.startVertIndex + v].vPosition);
            }
            
            // Лямбда-функция для честной планарной проекции 3D-точки вершины в 2D-пиксели атласа
            auto ProjectVertexToAtlas = [&](const glm::vec3& pos) -> glm::vec2
            {
                glm::vec3 localPos = pos - faceMinBounds; // Локальные 3D-координаты относительно угла грани
                float localU = 0.0f;
                float localV = 0.0f;
                
                // Выбираем оси проекции в зависимости от того, куда смотрит нормаль
                if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
                    localU = localPos.y; localV = localPos.z; // Стена вдоль YZ
                } else if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {
                    localU = localPos.x; localV = localPos.z; // Стена вдоль XZ
                } else {
                    localU = localPos.x; localV = localPos.y; // Пол / Потолок вдоль XY
                }
                
                // Переводим юниты в пиксели атласа на основе плотности и добавляем позицию полки упаковщика
                float pixelX = static_cast<float>(rect.x) + (localU * texelsPerUnit);
                float pixelY = static_cast<float>(rect.y) + (localV * texelsPerUnit);
                return glm::vec2(pixelX, pixelY);
            };
            
            // Обходим треугольники грани
            for (int i = 0; i < face.numOfIndices; i += 3)
            {
                unsigned int idx0 = bsp->m_indices[face.startIndex + i] + face.startVertIndex;
                unsigned int idx1 = bsp->m_indices[face.startIndex + i + 1] + face.startVertIndex;
                unsigned int idx2 = bsp->m_indices[face.startIndex + i + 2] + face.startVertIndex;
                
                tBSPVertex& v0 = bsp->m_verts[idx0];
                tBSPVertex& v1 = bsp->m_verts[idx1];
                tBSPVertex& v2 = bsp->m_verts[idx2];
                
                // Получаем пиксельные координаты вершин в атласе через проекцию
                glm::vec2 pixelUV0 = ProjectVertexToAtlas(v0.vPosition);
                glm::vec2 pixelUV1 = ProjectVertexToAtlas(v1.vPosition);
                glm::vec2 pixelUV2 = ProjectVertexToAtlas(v2.vPosition);
                
                // ЗАПИСЫВАЕМ НОВЫЕ НОРМАЛИЗОВАННЫЕ UV ДЛЯ ШЕЙДЕРА [0..1]
                if (!processedVertices[idx0]) {
                    v0.vLightmapCoord = pixelUV0 / glm::vec2(outAtlas.width, outAtlas.height);
                    processedVertices[idx0] = true;
                }
                if (!processedVertices[idx1]) {
                    v1.vLightmapCoord = pixelUV1 / glm::vec2(outAtlas.width, outAtlas.height);
                    processedVertices[idx1] = true;
                }
                if (!processedVertices[idx2]) {
                    v2.vLightmapCoord = pixelUV2 / glm::vec2(outAtlas.width, outAtlas.height);
                    processedVertices[idx2] = true;
                }
                
                // Рамка треугольника в атласе для растеризации
                int minX = std::max(0, static_cast<int>(std::floor(std::min({pixelUV0.x, pixelUV1.x, pixelUV2.x}))));
                int maxX = std::min(outAtlas.width - 1, static_cast<int>(std::ceil(std::max({pixelUV0.x, pixelUV1.x, pixelUV2.x}))));
                int minY = std::max(0, static_cast<int>(std::floor(std::min({pixelUV0.y, pixelUV1.y, pixelUV2.y}))));
                int maxY = std::min(outAtlas.height - 1, static_cast<int>(std::ceil(std::max({pixelUV0.y, pixelUV1.y, pixelUV2.y}))));
                
                for (int y = minY; y <= maxY; ++y)
                {
                    for (int x = minX; x <= maxX; ++x)
                    {
                        glm::vec2 pixelPos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                        glm::vec3 weights;
                        
                        if (ComputeBarycentric2D(pixelPos, pixelUV0, pixelUV1, pixelUV2, weights))
                        {
                            // Восстановление 3D позиции текселя в мире
                            glm::vec3 rawWorldPos = weights.x * v0.vPosition + weights.y * v1.vPosition + weights.z * v2.vPosition;
                            
                            // Ортогональная проекция точки на стену
                            glm::vec3 v = rawWorldPos - v0.vPosition;
                            float distanceError = glm::dot(v, texelNormal);
                            glm::vec3 texelWorldPos = rawWorldPos - (distanceError * texelNormal);
                            
                            // Смещение против самозатенения
                            texelWorldPos += texelNormal * 0.5f;
                            
                            glm::vec3 accumulatedColor(0.0f);
                            
                            for (const auto& light : lights)
                            {
                                if (light.isSun)
                                {
                                    float cosTheta = glm::max(glm::dot(texelNormal, light.dir), 0.0f);
                                    glm::vec3 end = texelWorldPos + light.dir * 4096.f;
                                    
                                    if (cosTheta > 0.0f)
                                    {
                                        if (TraceRay(bsp, texelWorldPos, end))
                                        {
                                            accumulatedColor += light.color * cosTheta * 10.0f;
                                        }
                                    }
                                }
                                else
                                {
                                    glm::vec3 lightDir = light.position - texelWorldPos;
                                    
                                    float dist = glm::length(lightDir);
                                    if (dist < 1.0f) continue;
                                    
                                    if (dist > light.maxDist) continue;
                                    
                                    lightDir = glm::normalize(lightDir);
                                    float cosTheta = glm::max(glm::dot(texelNormal, lightDir), 0.0f);
                                    
                                    if (cosTheta > 0.0f)
                                    {
                                        if (TraceRay(bsp, texelWorldPos, light.position))
                                        {
                                            float finalDist = std::max(dist, 64.0f);
                                            float attenuation = 64.0f / (finalDist * finalDist);
                                            accumulatedColor += light.color * attenuation * cosTheta;
                                        }
                                    }
                                }
                            }
                            
//                            accumulatedColor = accumulatedColor * 0.01f;
                            
//                            accumulatedColor = accumulatedColor / (1.0f + accumulatedColor);
                            
                            // 1. Получили ваш линейный цвет (уже умноженный на 100 на первом шаге)
                            glm::vec3 linear_rgb = accumulatedColor;
                            
                            // 2. Считаем общую яркость (Luminance) по стандарту ITU-R BT.709
                            float luma = dot(linear_rgb, glm::vec3(0.2126f, 0.7152f, 0.0722f));
                            
                            // Защита от деления на ноль, если в точке полная тьма
                            if (luma > 0.0001f) {
                                // 3. Сжимаем ТОЛЬКО скалярную яркость по Райнхарду
                                float compressed_luma = luma / (luma + 1.0f);
                                
                                // 4. Находим коэффициент изменения яркости
                                float scale = compressed_luma / luma;
                                
                                // 5. Умножаем исходный цвет на этот коэффициент
                                // Пропорции между R, G и B останутся ИДЕАЛЬНО ТЕМИ ЖЕ! Насыщенность сохранится.
                                linear_rgb *= scale;
                            }
                            
                            // 6. Обязательный Clamping (на всякий случай, чтобы float не вылез за 1.0)
                            accumulatedColor.x = std::min(linear_rgb.x, 1.0f);
                            accumulatedColor.y = std::min(linear_rgb.y, 1.0f);
                            accumulatedColor.z = std::min(linear_rgb.z, 1.0f);
                            
                            
//                            {
//                                float max = accumulatedColor.x > accumulatedColor.y ? accumulatedColor.x : accumulatedColor.y;
//                                max = max > accumulatedColor.z ? max : accumulatedColor.z;
//                                max = max > 1 ? max : 1;
//                                
//                                accumulatedColor.x = accumulatedColor.x / max;
//                                accumulatedColor.y = accumulatedColor.y / max;
//                                accumulatedColor.z = accumulatedColor.z / max;
//                            }
                            
                            size_t pixelIndex = (y * outAtlas.width + x) * 3;
                            
                            float gamma = 1.0f/2.2f;
                            accumulatedColor = glm::pow(accumulatedColor, glm::vec3(gamma));
                            
                            // Записываем финальный цвет
                            outAtlas.buffer[pixelIndex + 0] = static_cast<unsigned char>(accumulatedColor.x * 255.0f);
                            outAtlas.buffer[pixelIndex + 1] = static_cast<unsigned char>(accumulatedColor.y * 255.0f);
                            outAtlas.buffer[pixelIndex + 2] = static_cast<unsigned char>(accumulatedColor.z * 255.0f);
                        }
                    }
                }
            }
        }
        
        // =====================================================================
        // ДИЛАТАЦИЯ (Убираем темные полосы на стыках / Texel Bleeding)
        // =====================================================================
        // Создаем временную копию буфера для безопасного чтения
        std::vector<unsigned char> tempBuffer(outAtlas.width * outAtlas.height * 3);
        std::copy(outAtlas.buffer, outAtlas.buffer + (outAtlas.width * outAtlas.height * 3), tempBuffer.begin());
        
        // Направления для поиска соседних пикселей (8 направлений)
        int dx[] = {-1,  1,  0,  0, -1, -1,  1,  1};
        int dy[] = { 0,  0, -1,  1, -1,  1, -1,  1};
        
        // Делаем несколько итераций расширения (равное половине вашего padding, например 2-3 итерации)
        for (int iteration = 0; iteration < 2; ++iteration) {
            for (int y = 1; y < outAtlas.height - 1; ++y) {
                for (int x = 1; x < outAtlas.width - 1; ++x) {
                    size_t idx = (y * outAtlas.width + x) * 3;
                    
                    // Если текущий пиксель в атласе черный (не принадлежащий геометрии)
                    if (tempBuffer[idx + 0] == 0 && tempBuffer[idx + 1] == 0 && tempBuffer[idx + 2] == 0) {
                        
                        int sumR = 0, sumG = 0, sumB = 0;
                        int count = 0;
                        
                        // Ищем цветных соседей вокруг
                        for (int k = 0; k < 8; ++k) {
                            int nx = x + dx[k];
                            int ny = y + dy[k];
                            size_t nIdx = (ny * outAtlas.width + nx) * 3;
                            
                            // Если сосед не черный, учитываем его цвет
                            if (tempBuffer[nIdx + 0] != 0 || tempBuffer[nIdx + 1] != 0 || tempBuffer[nIdx + 2] != 0) {
                                sumR += tempBuffer[nIdx + 0];
                                sumG += tempBuffer[nIdx + 1];
                                sumB += tempBuffer[nIdx + 2];
                                count++;
                            }
                        }
                        
                        // Если нашли хоть одного цветного соседа — размазываем его цвет в текущий пиксель
                        if (count > 0) {
                            outAtlas.buffer[idx + 0] = sumR / count;
                            outAtlas.buffer[idx + 1] = sumG / count;
                            outAtlas.buffer[idx + 2] = sumB / count;
                        }
                    }
                }
            }
            
            // Обновляем временный буфер для следующей итерации расширения
            std::copy(outAtlas.buffer, outAtlas.buffer + (outAtlas.width * outAtlas.height * 3), tempBuffer.begin());
        }
    }
}
