//
//  TextureLibrary.cpp
//  Cucumber
//

#include "TextureLibrary.h"

#include "ProjectManager.h"
#include "RenderDevice.h"
#include "TextureManager.h"

// Реализация stb_image подключена в другом .cpp, как и у TextureManager.cpp. Путь поправьте под расположение файла.
#include "../vendor/stb_image.h"

#include <algorithm>
#include <cctype>

namespace
{
    // Порядок совпадает со списком в TextureManager::loadTexture: при одинаковом имени побеждает меньший ранг.
    // Если добавляете формат, например .bmp, добавьте его в обоих местах. -1: не картинка.
    int ExtensionRank(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".png")  return 0;
        if (ext == ".jpg")  return 1;
        if (ext == ".tga")  return 2;
        if (ext == ".jpeg") return 3;
        return -1;
    }

    bool IsHidden(const std::filesystem::path& path)
    {
        const std::string name = path.filename().string();
        return !name.empty() && name[0] == '.';
    }
}

TextureLibrary& TextureLibrary::Get()
{
    static TextureLibrary instance;
    return instance;
}

TextureLibrary::~TextureLibrary()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    for (std::thread& worker : m_workers) worker.join();
}

// ------------------------------------------------------------------------------------------------ индекс

const std::vector<TextureLibrary::Entry>& TextureLibrary::entries()
{
    ensureIndex();
    return m_entries;
}

void TextureLibrary::refresh()
{
    m_indexed = false;
    m_info.clear();
    ++m_revision;

    // Превью сбрасываем целиком: файлы могли измениться. Уже запрошенные задания устарели (поколение), очередь чистим.
    for (auto& item : m_previews) releasePreview(item.second);
    m_previews.clear();
    ++m_generation;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.clear();
    }
    // Индекс пересоберётся при ближайшем обращении: refresh() зовут после каждой файловой операции в AssetsPanel,
    // и сканировать диск каждый раз, когда никто не смотрит на текстуры, незачем
}

bool TextureLibrary::contains(const std::string& name)
{
    ensureIndex();
    return m_indexByName.count(name) != 0;
}

void TextureLibrary::ensureIndex()
{
    if (m_indexed) return;
    m_indexed = true;

    m_entries.clear();
    m_indexByName.clear();
    m_root = ProjectManager::Get().assetsPath();

    std::error_code ec;
    if (!std::filesystem::exists(m_root, ec)) return;

    struct Candidate { Entry entry; int rank; };
    std::unordered_map<std::string, Candidate> byName;

    for (std::filesystem::recursive_directory_iterator it(m_root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
    {
        // Скрытые каталоги (.git и подобные) не заходим
        if (IsHidden(it->path()))
        {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            ec.clear();
            continue;
        }

        const int rank = ExtensionRank(it->path());
        if (rank < 0 || !it->is_regular_file(ec)) { ec.clear(); continue; }

        std::filesystem::path relative = std::filesystem::relative(it->path(), m_root, ec);
        if (ec) { ec.clear(); continue; }

        Entry entry;
        entry.file = relative.generic_string();
        entry.folder = relative.parent_path().generic_string();
        relative.replace_extension();
        entry.name = relative.generic_string();

        // Одинаковое имя при разных расширениях (foo.png и foo.jpg): берём тот же файл, что выберет TextureManager
        const auto existing = byName.find(entry.name);
        if (existing == byName.end())
        {
            const std::string key = entry.name;
            byName.emplace(key, Candidate{ std::move(entry), rank });
        }
        else if (rank < existing->second.rank)
        {
            existing->second = Candidate{ std::move(entry), rank };
        }
    }

    m_entries.reserve(byName.size());
    for (auto& item : byName) m_entries.push_back(std::move(item.second.entry));
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });

    for (size_t i = 0; i < m_entries.size(); ++i) m_indexByName.emplace(m_entries[i].name, i);
}

// ------------------------------------------------------------------------------------------ полноразмерные

const TextureLibrary::Info& TextureLibrary::info(const std::string& name)
{
    const auto it = m_info.find(name);
    if (it != m_info.end()) return it->second;

    Info info;
    info.found = contains(name);

    // Нет файла: TextureManager вернёт запасную текстуру, берём её размеры и хэндл (так материал не остаётся пустым)
    if (auto asset = TextureManager::Get().loadTexture(name))
    {
        info.width = asset->getWidth();
        info.height = asset->getHeight();
        info.handle = asset->getHandle();
    }
    return m_info.emplace(name, info).first->second;
}

// ---------------------------------------------------------------------------------------------- превью

const TextureLibrary::Preview& TextureLibrary::preview(const std::string& name)
{
    const auto existing = m_previews.find(name);
    if (existing != m_previews.end())
    {
        existing->second.lastUsed = m_frame;
        return existing->second.preview;
    }

    PreviewSlot slot;
    slot.lastUsed = m_frame;

    ensureIndex();
    const auto indexed = m_indexByName.find(name);
    if (indexed == m_indexByName.end())
    {
        slot.preview.state = Preview::State::NotFound;
    }
    else
    {
        slot.preview.state = Preview::State::Loading;

        Job job;
        job.name = name;
        job.path = m_root / m_entries[indexed->second].file;
        job.generation = m_generation;

        startWorkers();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push_back(std::move(job));
        }
        m_wake.notify_one();
    }
    return m_previews.emplace(name, std::move(slot)).first->second.preview;
}

void TextureLibrary::pumpPreviews(uint64_t frame)
{
    m_frame = frame;
    collectGarbage();

    // 1. Готовые результаты в GPU, не больше нескольких за кадр: заливка и мипмапы не бесплатны
    for (int uploaded = 0; uploaded < kMaxPreviewUploadsPerFrame;)
    {
        Result result;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_results.empty()) break;
            result = std::move(m_results.front());
            m_results.pop_front();
        }

        if (result.generation != m_generation) continue;   // заказано до refresh()

        const auto it = m_previews.find(result.name);
        if (it == m_previews.end() || it->second.preview.state != Preview::State::Loading) continue;

        PreviewSlot& slot = it->second;
        if (!result.ok)
        {
            slot.preview.state = Preview::State::Failed;
            continue;
        }

        slot.preview.handle = RenderDevice::makeTexture(result.previewWidth, result.previewHeight, true, true, result.pixels.data());
        slot.preview.width = result.width;
        slot.preview.height = result.height;
        slot.preview.state = Preview::State::Ready;
        slot.bytes = static_cast<size_t>(result.previewWidth) * result.previewHeight * 4 * 4 / 3;   // с мипмапами
        m_previewBytes += slot.bytes;
        ++uploaded;
    }

    // 2. Задания, которые уже не нужны (прокрутили дальше), снимаем с очереди: иначе при быстрой прокрутке
    //    потоки будут декодировать картинки, которых никто не увидит
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_jobs.begin(); it != m_jobs.end();)
        {
            const auto slot = m_previews.find(it->name);
            const bool stale = slot == m_previews.end() || slot->second.lastUsed + 2 < frame;
            if (!stale) { ++it; continue; }

            if (slot != m_previews.end() && slot->second.preview.state == Preview::State::Loading) m_previews.erase(slot);
            it = m_jobs.erase(it);
        }
    }

    // 3. Лишнее по бюджету
    evictOverBudget();
}

void TextureLibrary::releasePreview(PreviewSlot& slot)
{
    // Удаляем не сразу: ImGui рисует собранные за кадр команды уже после того, как панели отработали,
    // и текстура, показанная в этом кадре, должна дожить до его конца
    if (slot.preview.handle != 0) m_graveyard.emplace_back(slot.preview.handle, m_frame);
    m_previewBytes -= std::min(m_previewBytes, slot.bytes);
    slot.preview.handle = 0;
    slot.bytes = 0;
}

void TextureLibrary::collectGarbage()
{
    for (size_t i = 0; i < m_graveyard.size();)
    {
        if (m_graveyard[i].second + 2 <= m_frame)
        {
            RenderDevice::deleteTexture(m_graveyard[i].first);
            m_graveyard[i] = m_graveyard.back();
            m_graveyard.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

void TextureLibrary::evictOverBudget()
{
    if (m_previewBytes <= kPreviewBudgetBytes) return;

    // Кандидаты: готовые превью, которые не использовались последние кадры. Бюджет мягкий: если всё
    // выбрано в кадре (огромная сетка мелких ячеек), выгружать нечего.
    std::vector<std::pair<uint64_t, std::string>> candidates;
    for (const auto& item : m_previews)
    {
        const PreviewSlot& slot = item.second;
        if (slot.preview.state == Preview::State::Ready && slot.lastUsed + 2 < m_frame)
            candidates.emplace_back(slot.lastUsed, item.first);
    }
    std::sort(candidates.begin(), candidates.end());   // самые давние первыми

    for (const auto& candidate : candidates)
    {
        if (m_previewBytes <= kPreviewBudgetBytes) break;
        const auto it = m_previews.find(candidate.second);
        releasePreview(it->second);
        m_previews.erase(it);
    }
}

// ----------------------------------------------------------------------------------------- фоновые потоки

void TextureLibrary::startWorkers()
{
    if (!m_workers.empty()) return;
    for (int i = 0; i < kPreviewWorkers; ++i) m_workers.emplace_back([this] { workerLoop(); });
}

void TextureLibrary::workerLoop()
{
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this] { return m_stop || !m_jobs.empty(); });
            if (m_stop) return;
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }

        Result result = Decode(job);   // GL здесь не трогаем: только диск и процессор

        std::lock_guard<std::mutex> lock(m_mutex);
        m_results.push_back(std::move(result));
    }
}

TextureLibrary::Result TextureLibrary::Decode(const Job& job)
{
    Result result;
    result.name = job.name;
    result.generation = job.generation;

    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(job.path.string().c_str(), &width, &height, &channels, 4);
    if (!data || width <= 0 || height <= 0)
    {
        if (data) stbi_image_free(data);
        return result;
    }

    // Уменьшение усреднением целых блоков factor x factor: для превью достаточно, и без сторонних библиотек
    const int factor = std::max(1, (std::max(width, height) + kPreviewEdge - 1) / kPreviewEdge);
    const int pw = (width + factor - 1) / factor;
    const int ph = (height + factor - 1) / factor;

    result.pixels.resize(static_cast<size_t>(pw) * ph * 4);
    if (factor == 1)
    {
        std::copy(data, data + result.pixels.size(), result.pixels.begin());
    }
    else
    {
        for (int y = 0; y < ph; ++y)
        {
            const int y0 = y * factor;
            const int y1 = std::min(height, y0 + factor);
            for (int x = 0; x < pw; ++x)
            {
                const int x0 = x * factor;
                const int x1 = std::min(width, x0 + factor);

                uint32_t sum[4] = { 0, 0, 0, 0 };
                for (int yy = y0; yy < y1; ++yy)
                {
                    const unsigned char* p = data + (static_cast<size_t>(yy) * width + x0) * 4;
                    for (int xx = x0; xx < x1; ++xx, p += 4)
                    {
                        sum[0] += p[0]; sum[1] += p[1]; sum[2] += p[2]; sum[3] += p[3];
                    }
                }

                const uint32_t count = static_cast<uint32_t>((y1 - y0) * (x1 - x0));
                unsigned char* out = result.pixels.data() + (static_cast<size_t>(y) * pw + x) * 4;
                for (int c = 0; c < 4; ++c) out[c] = static_cast<unsigned char>((sum[c] + count / 2) / count);
            }
        }
    }
    stbi_image_free(data);

    result.ok = true;
    result.width = width;
    result.height = height;
    result.previewWidth = pw;
    result.previewHeight = ph;
    return result;
}

// --------------------------------------------------------------------------------------------- имена

bool TextureLibrary::IsReservedMaterial(const std::string& name)
{
    return name == "__TB_empty" || name == "skip" || name == "clip" || name == "default";
}

std::string TextureLibrary::CanonicalName(const std::string& legacyName, const std::filesystem::path& texturesRoot)
{
    if (IsReservedMaterial(legacyName)) return legacyName;

    std::error_code ec;
    const std::filesystem::path prefix = std::filesystem::relative(texturesRoot, ProjectManager::Get().assetsPath(), ec);

    // Каталог текстур вне assets (или путь не удалось вычислить): выразить имя нельзя, оставляем прежнее
    if (ec || prefix.empty() || *prefix.begin() == "..") return legacyName;

    return (prefix / legacyName).lexically_normal().generic_string();
}
