//
//  LightmapBakeService.h
//  Cucumber
//
//  Фоновый запуск бейка: редактор не замирает, прогресс виден, задачу можно отменить.
//  Все вызовы — из главного потока. Бейк идёт в своём потоке (и внутри раскладывается на все ядра),
//  результат отдаётся в onDone тоже в главном потоке, внутри update(): там безопасно трогать GL.
//
//  Одновременно выполняется одна задача, остальные ждут очереди (бейк и так занимает все ядра).
//

#pragma once

#include "LightmapBaker.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class LightmapBakeService
{
public:
    using Completion = std::function<void(bool ok, LightmapBakeResult&& result, const std::string& error)>;

    static LightmapBakeService& Get();

    // Возвращает id задачи. Отменённая задача onDone не вызывает.
    uint64_t submit(LightmapBakeInput input, Completion onDone);
    void cancel(uint64_t id);

    // Раз за кадр из главного потока (Application::run): запускает следующую задачу и вызывает onDone завершённых.
    void update();

    bool isActive(uint64_t id) const;        // в очереди или выполняется
    float progress(uint64_t id) const;       // 0..1, для задачи в очереди 0

    ~LightmapBakeService();

private:
    LightmapBakeService() = default;

    struct Job
    {
        uint64_t id = 0;
        LightmapBakeInput input;
        Completion onDone;
        LightmapBakeControl control;

        std::thread thread;
        std::atomic<bool> finished{false};
        bool started = false;
        bool cancelled = false;

        // Пишет рабочий поток; главный читает только после finished == true
        bool ok = false;
        LightmapBakeResult result;
        std::string error;
    };

    std::vector<std::unique_ptr<Job>> m_jobs;
    uint64_t m_nextId = 1;
};
