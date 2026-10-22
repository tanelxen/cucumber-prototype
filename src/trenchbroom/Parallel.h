//
//  Parallel.h
//  Cucumber
//
//  Минимальный параллельный for без зависимостей.
//

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// Делит [0, count) на куски по chunkSize и раздаёт их потокам; fn(begin, end) вызывается из РАЗНЫХ потоков.
// Всё, что нельзя делить между потоками (например, nanort::TriangleIntersector с изменяемым состоянием луча),
// создавайте внутри fn. maxThreads == 0 -> hardware_concurrency. Блокирует до завершения всех кусков.
template <typename Fn>
void ParallelForChunks(size_t count, size_t chunkSize, unsigned maxThreads, Fn&& fn)
{
    if (count == 0) return;

    chunkSize = std::max<size_t>(1, chunkSize);
    const size_t chunks = (count + chunkSize - 1) / chunkSize;

    unsigned threads = maxThreads ? maxThreads : std::max(1u, std::thread::hardware_concurrency());
    threads = static_cast<unsigned>(std::min<size_t>(threads, chunks));

    std::atomic<size_t> next{0};
    const auto worker = [&]()
    {
        for (;;)
        {
            const size_t chunk = next.fetch_add(1);
            if (chunk >= chunks) break;

            const size_t begin = chunk * chunkSize;
            fn(begin, std::min(count, begin + chunkSize));
        }
    };

    if (threads <= 1)
    {
        worker();
        return;
    }

    std::vector<std::thread> pool;
    pool.reserve(threads - 1);
    for (unsigned t = 1; t < threads; ++t) pool.emplace_back(worker);
    worker();   // текущий поток тоже работает
    for (auto& thread : pool) thread.join();
}
