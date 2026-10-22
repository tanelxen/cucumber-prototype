//
//  LightmapBakeService.cpp
//  Cucumber
//

#include "LightmapBakeService.h"

#include <algorithm>
#include <utility>

LightmapBakeService& LightmapBakeService::Get()
{
    static LightmapBakeService service;
    return service;
}

LightmapBakeService::~LightmapBakeService()
{
    for (auto& job : m_jobs)
    {
        job->control.cancel.store(true);
        if (job->started && job->thread.joinable()) job->thread.join();
    }
}

uint64_t LightmapBakeService::submit(LightmapBakeInput input, Completion onDone)
{
    auto job = std::make_unique<Job>();
    job->id = m_nextId++;
    job->input = std::move(input);
    job->onDone = std::move(onDone);

    const uint64_t id = job->id;
    m_jobs.push_back(std::move(job));
    return id;
}

void LightmapBakeService::cancel(uint64_t id)
{
    for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it)
    {
        Job& job = **it;
        if (job.id != id) continue;

        if (!job.started)
        {
            m_jobs.erase(it);   // ещё не начинали — просто убираем
        }
        else
        {
            job.cancelled = true;
            job.control.cancel.store(true);   // поток завершится сам, update() выбросит результат
        }
        return;
    }
}

void LightmapBakeService::update()
{
    // 1) Собираем завершённые. Колбэки вызываем ПОСЛЕ изменения списка: они могут звать submit/cancel.
    std::vector<std::unique_ptr<Job>> finished;
    for (auto it = m_jobs.begin(); it != m_jobs.end();)
    {
        Job& job = **it;
        if (job.started && job.finished.load(std::memory_order_acquire))
        {
            job.thread.join();
            finished.push_back(std::move(*it));
            it = m_jobs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 2) Если ничего не выполняется — запускаем следующую
    const bool running = std::any_of(m_jobs.begin(), m_jobs.end(), [](const auto& job) { return job->started; });
    if (!running)
    {
        for (auto& job : m_jobs)
        {
            if (job->started) continue;

            Job* raw = job.get();
            raw->started = true;
            raw->thread = std::thread([raw]()
            {
                raw->ok = LightmapBaker::bake(raw->input, raw->result, raw->error, &raw->control);
                raw->input = {};   // освобождаем копию геометрии, она больше не нужна
                raw->finished.store(true, std::memory_order_release);
            });
            break;
        }
    }

    // 3) Колбэки завершённых (главный поток: здесь можно загружать текстуры в GL)
    for (auto& job : finished)
    {
        if (!job->cancelled && job->onDone)
        {
            job->onDone(job->ok, std::move(job->result), job->error);
        }
    }
}

bool LightmapBakeService::isActive(uint64_t id) const
{
    return std::any_of(m_jobs.begin(), m_jobs.end(), [id](const auto& job) { return job->id == id && !job->cancelled; });
}

float LightmapBakeService::progress(uint64_t id) const
{
    for (const auto& job : m_jobs)
    {
        if (job->id == id) return job->control.progress.load(std::memory_order_relaxed);
    }
    return 0.0f;
}
