//
//  LevelEditSession.h
//  Cucumber
//
//  Состояние одного сеанса правки геометрии: история, выделение, наведение, активный инструмент. Живёт, пока
//  уровень в режиме Edit (владеет им MapComponent). Про OpenGL, ImGui и рендер не знает: ввод приходит в виде
//  ViewportPointer, результат правок уходит потребителям через ChangedCallback (ChangeSet).
//

#pragma once

#include "LevelHistory.h"
#include "LevelPicking.h"
#include "LevelTool.h"
#include "ViewportPointer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Грань уровня: солид + грань внутри него (uid стабильны при правках формы и undo)
struct FaceRef
{
    uint64_t solidUid = 0;
    uint64_t faceUid = 0;

    bool operator==(const FaceRef& other) const { return solidUid == other.solidUid && faceUid == other.faceUid; }
};

enum class ToolType
{
    Select = 0,
    Block,
    Vertex,
    Count
};

class LevelEditSession
{
public:
    using ChangedCallback = std::function<void(const ChangeSet&)>;

    // document должен пережить сессию
    LevelEditSession(LevelDocument& document, ChangedCallback onChanged);
    ~LevelEditSession();

    LevelDocument& document() { return m_document; }
    const LevelDocument& document() const { return m_document; }
    LevelHistory& history() { return m_history; }
    const LevelHistory& history() const { return m_history; }

    double gridSize() const;   // шаг сетки документа (всегда > 0)

    // --- Правки (все проходят через историю) ---
    // coalesceTag: см. LevelHistory::commit (перетаскивание ползунка = одна запись undo)
    bool commit(std::string name, std::vector<SolidChange> changes, const std::string& coalesceTag = {});
    void sealEdit() { m_history.sealLast(); }   // конец перетаскивания: следующая правка начнёт новую запись undo
    // undo / redo / deleteSelection / translateSelection не выполняются, пока инструмент в середине операции
    bool undo();
    bool redo();

    bool deleteSelection();
    // Сдвиг выделенных солидов. textureLock: текстура остаётся на месте относительно тела (компенсируется shift).
    bool translateSelection(const glm::vec3& delta, bool textureLock = true);

    // --- Выделение ---
    const std::vector<uint64_t>& selectedSolids() const { return m_selectedSolids; }
    bool isSolidSelected(uint64_t uid) const;
    void clearSelection();
    void selectSolid(uint64_t uid, bool toggle);
    const Solid* findSolid(uint64_t uid) const;

    // Выбор граней (взаимоисключающ с выбором солидов). toggle добавляет / убирает грань, иначе выбор заменяется.
    const std::vector<FaceRef>& selectedFaces() const { return m_selectedFaces; }
    bool isFaceSelected(uint64_t faceUid) const;
    void selectFace(uint64_t solidUid, uint64_t faceUid, bool toggle = false);
    const Face* findFace(const FaceRef& ref) const;

    // Грани, к которым применяется текстурирование: выбранные грани, а если их нет, все грани выбранных солидов
    std::vector<FaceRef> textureTargets() const;

    // Правка граней: fn вызывается для каждой грани (solid это тело до правки) и возвращает true, если грань изменена.
    // Все изменённые тела уходят одной правкой в историю. false, если ничего не изменилось.
    using FaceEditFn = std::function<bool(const Solid& solid, Face& face)>;
    bool editFaces(const std::vector<FaceRef>& faces, std::string name, std::string coalesceTag, const FaceEditFn& fn);

    // Материал для новых тел: у первой выбранной грани, иначе у выделенного солида, иначе самый частый в документе.
    // Если подсказок нет: первый материал таблицы, а при пустой таблице добавляется "default".
    uint32_t currentMaterial();

    // --- Инструменты ---
    void setTool(ToolType type);              // прерывает операцию прежнего инструмента
    ToolType activeToolType() const { return m_active; }
    LevelTool& activeTool() { return *m_tools[static_cast<size_t>(m_active)]; }
    const LevelTool& activeTool() const { return *m_tools[static_cast<size_t>(m_active)]; }
    bool isToolBusy() const { return activeTool().isBusy(); }

    // Клавиша активному инструменту (панель уже отфильтровала фокус, текстовый ввод и полёт камеры)
    bool handleKey(LevelKey key, bool shift) { return activeTool().onKey(*this, key, shift); }

    // Esc: прервать операцию; иначе вернуться к Select; иначе снять выделение. true, если что-то сделано.
    bool escape();

    // --- Ввод ---
    // Вызывать для каждого вида каждый кадр (с over = false тоже). Наведение привязано к виду, который его
    // выставил: вид, над которым курсора нет, чужое наведение не сбрасывает (панелей несколько).
    void handlePointer(uint32_t viewId, const ViewportPointer& pointer);
    bool hasHover() const { return m_hasHover; }
    const LevelHit& hover() const { return m_hover; }

    const std::string& lastError() const { return m_lastError; }
    void reportError(std::string message) { m_lastError = std::move(message); }

private:
    void notify(const ChangeSet& changes);
    void pruneSelection();
    void updateHover(uint32_t viewId, const ViewportPointer& pointer);

    LevelDocument& m_document;
    LevelHistory m_history;
    ChangedCallback m_onChanged;

    std::unique_ptr<LevelTool> m_tools[static_cast<size_t>(ToolType::Count)];
    ToolType m_active = ToolType::Select;

    std::vector<uint64_t> m_selectedSolids;
    std::vector<FaceRef> m_selectedFaces;

    LevelHit m_hover;
    bool m_hasHover = false;
    uint32_t m_hoverView = 0;

    std::string m_lastError;
};

// Копия солида, сдвинутая на delta (с компенсацией UV при textureLock)
Solid TranslatedSolid(const Solid& solid, const glm::vec3& delta, bool textureLock);
