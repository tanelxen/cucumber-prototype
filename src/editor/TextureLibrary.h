//
//  TextureLibrary.h
//  Cucumber
//
//  Единственное место, где имя материала превращается в текстуру.
//
//  Каноническое имя материала: путь к картинке относительно корня assets, без расширения и со слешами '/',
//  например "wolf/textures/e_wall/brick" или "props/crate". Расширение не входит в имя (как и раньше, когда
//  имена приходили из TrenchBroom), поэтому две картинки с одним именем и разными расширениями в одной папке
//  считаются одной текстурой, как и в TextureManager (приоритет расширений: png, jpg, tga, jpeg).
//
//  Библиотека держит три независимых вещи:
//    - индекс файлов на диске (entries/contains): строится лениво, обновляется refresh();
//    - полноразмерные текстуры (info): размер и GL-хэндл поверх TextureManager. Нужны уровню и Fit to face.
//      TextureManager их не выгружает, поэтому для просмотра каталога info() НЕ годится;
//    - кэш превью (preview): уменьшенные копии для браузеров. Декодируются в фоновых потоках, в GL заливаются
//      в главном потоке по несколько штук за кадр, при превышении бюджета памяти выгружаются по давности.
//
//  Все публичные методы вызываются только из главного потока (GL).
//

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

class TextureLibrary
{
public:
    struct Entry
    {
        std::string name;     // каноническое имя (см. выше)
        std::string folder;   // каталог относительно assets, "" для корня; для фильтра по папкам в браузере
        std::string file;     // путь к файлу относительно assets, с расширением
    };

    struct Info
    {
        int width = 256;      // файла нет: считаем 256 x 256, как раньше в MapMesh
        int height = 256;
        unsigned int handle = 0;
        bool found = false;   // файл есть в индексе (иначе в handle лежит запасная текстура TextureManager)
    };

    struct Preview
    {
        enum class State
        {
            Loading,    // декодируется в фоне или ждёт заливки в GPU: рисуйте заглушку
            Ready,      // handle действителен; width и height это размер оригинала
            Failed,     // файл есть, но не прочитался
            NotFound    // файла нет в индексе
        };

        State state = State::NotFound;
        unsigned int handle = 0;   // GL-текстура превью (не оригинал), не более kPreviewEdge по большей стороне
        int width = 0;
        int height = 0;
    };

    static constexpr int kPreviewEdge = 192;                          // большая сторона превью, пикселей
    static constexpr size_t kPreviewBudgetBytes = 64u * 1024u * 1024u;  // мягкий лимит GPU-памяти под превью
    static constexpr int kMaxPreviewUploadsPerFrame = 4;
    static constexpr int kPreviewWorkers = 2;

    static TextureLibrary& Get();

    ~TextureLibrary();

    // Все картинки проекта, отсортированы по имени
    const std::vector<Entry>& entries();

    // Сбросить кэши сведений и превью, индекс пересоберётся при ближайшем обращении (появились, переименованы или
    // подменены файлы). Дёшево: диск не сканируется, пока кто-нибудь не запросит список или превью.
    void refresh();

    // Растёт при каждом refresh(): по нему панели понимают, что их производные списки устарели
    uint64_t revision() const { return m_revision; }

    bool contains(const std::string& name);

    // Полноразмерная текстура. Нет файла: found == false, размеры и хэндл запасной текстуры. Ссылка действует до refresh().
    // Грузит файл синхронно и НАВСЕГДА (кэш TextureManager): для сеток и списков пользуйтесь preview().
    const Info& info(const std::string& name);

    // Превью для просмотра. Не блокирует: первый вызов ставит файл в очередь на фоновое декодирование и возвращает Loading.
    // Ссылка действует до следующего pumpPreviews() и refresh(). Использованное в кадре превью не выгружается.
    const Preview& preview(const std::string& name);

    // Ровно один раз за кадр, до отрисовки панелей (UIManager::drawPanels): заливает готовые превью в GPU, снимает из
    // очереди то, что давно не запрашивали (быстрая прокрутка), выгружает лишнее по бюджету.
    // frame: монотонный номер кадра (ImGui::GetFrameCount()).
    void pumpPreviews(uint64_t frame);

    // Имя из старого формата (путь относительно <texturesRoot>, как в .map и в .cmap.json версии 1) в каноническое.
    // texturesRoot: абсолютный путь к каталогу, от которого считались старые имена (<basePath>/textures).
    // Служебные имена (IsReservedMaterial) и уровни вне assets возвращаются как есть.
    static std::string CanonicalName(const std::string& legacyName, const std::filesystem::path& texturesRoot);

    // Имена без файла: skip, clip, __TB_empty (NoDraw) и "default" (материал новых тел)
    static bool IsReservedMaterial(const std::string& name);

private:
    TextureLibrary() = default;
    TextureLibrary(const TextureLibrary&) = delete;
    TextureLibrary& operator=(const TextureLibrary&) = delete;

    struct PreviewSlot
    {
        Preview preview;
        uint64_t lastUsed = 0;
        size_t bytes = 0;
    };

    struct Job
    {
        std::string name;
        std::filesystem::path path;
        uint64_t generation = 0;
    };

    struct Result
    {
        std::string name;
        uint64_t generation = 0;
        bool ok = false;
        int width = 0;            // размер оригинала
        int height = 0;
        int previewWidth = 0;
        int previewHeight = 0;
        std::vector<unsigned char> pixels;   // RGBA, previewWidth x previewHeight
    };

    void ensureIndex();

    void startWorkers();
    void workerLoop();
    static Result Decode(const Job& job);

    void releasePreview(PreviewSlot& slot);   // GL-текстура уходит в отложенное удаление
    void collectGarbage();
    void evictOverBudget();

    // Индекс
    std::vector<Entry> m_entries;
    std::unordered_map<std::string, size_t> m_indexByName;   // имя -> позиция в m_entries
    std::filesystem::path m_root;
    bool m_indexed = false;
    uint64_t m_revision = 1;

    // Полноразмерные
    std::unordered_map<std::string, Info> m_info;

    // Превью (главный поток)
    std::unordered_map<std::string, PreviewSlot> m_previews;
    uint64_t m_frame = 0;
    size_t m_previewBytes = 0;
    uint64_t m_generation = 1;                                // растёт при refresh(): результаты старых заданий отбрасываются
    std::vector<std::pair<unsigned int, uint64_t>> m_graveyard;   // (GL-текстура, кадр, когда сброшена)

    // Фоновые потоки
    std::mutex m_mutex;                                       // защищает m_jobs, m_results, m_stop
    std::condition_variable m_wake;
    std::deque<Job> m_jobs;
    std::deque<Result> m_results;
    std::vector<std::thread> m_workers;
    bool m_stop = false;
};
