//
//  LightmapController.h
//  Cucumber
//
//  Жизненный цикл лайтмапы одной карты: загрузка кэша с диска, фоновый бейк, сохранение результата,
//  определение "устарела ли лайтмапа". Вынесен из MapComponent, чтобы компонент не разрастался.
//
//  Статус пишется прямо в переданную строку (MapComponent::bakeStatus), поэтому Inspector менять не нужно.
//

#pragma once

#include "LightmapBaker.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class MapMesh;

class LightmapController
{
public:
    explicit LightmapController(std::string& statusOut) : m_status(statusOut) {}
    ~LightmapController();

    LightmapController(const LightmapController&) = delete;
    LightmapController& operator=(const LightmapController&) = delete;

    // Вызывать после каждой (пере)сборки меша карты. Отменяет бейк предыдущего меша и пробует загрузить кэш.
    void attach(std::shared_ptr<MapMesh> mesh, std::filesystem::path cachePath, int dilationPasses);

    // Запускает фоновый бейк и сразу возвращается. false — если запускать нечего/нечем (причина в статусе).
    bool startBake(std::vector<LightmapBakeLight> lights);
    void cancel();

    bool isBaking() const;
    float progress() const;

    // Лайтмапа не соответствует текущим лампам (или ещё не запекалась, а запечённые лампы есть).
    bool isOutdated(const std::vector<LightmapBakeLight>& currentLights) const;

private:
    LightmapBakeSettings makeSettings() const;

    std::string& m_status;
    std::shared_ptr<MapMesh> m_mesh;
    std::filesystem::path m_cachePath;
    int m_dilationPasses = 2;

    uint64_t m_geometryHash = 0;
    uint64_t m_bakedLightsHash = 0;   // 0 = лайтмапы нет
    uint64_t m_jobId = 0;

    // Колбэк бейка может прийти, когда контроллера уже нет: он хранит weak_ptr на этот токен
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
