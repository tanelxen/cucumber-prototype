//
//  LevelEditSession.h
//  Cucumber
//
//  Состояние одного сеанса правки геометрии: история, выделение, наведение. Живёт, пока уровень в режиме Edit
//  (владеет им MapComponent). Про OpenGL, ImGui и рендер не знает: ввод приходит в виде ViewportPointer,
//  результат правок уходит потребителям через ChangedCallback (ChangeSet).
//

#pragma once

#include "LevelHistory.h"
#include "LevelPicking.h"
#include "ViewDesc.h"

#include <functional>
#include <string>
#include <vector>

// Состояние указателя над одним видом за кадр. Заполняет панель вьюпорта.
struct ViewportPointer
{
    CameraData camera;
    glm::ivec2 sizePx{1, 1};
    glm::vec2 mousePx{0.0f};     // относительно левого верхнего угла вида
    bool over = false;           // вид вправе принять ввод: курсор над ним, не летим ПКМ, не тянем гизмо
    bool pressed = false;        // ЛКМ нажата именно в этом кадре
    bool down = false;
    bool released = false;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

class LevelEditSession
{
public:
    using ChangedCallback = std::function<void(const ChangeSet&)>;

    // document должен пережить сессию
    LevelEditSession(LevelDocument& document, ChangedCallback onChanged);

    LevelDocument& document() { return m_document; }
    const LevelDocument& document() const { return m_document; }
    LevelHistory& history() { return m_history; }
    const LevelHistory& history() const { return m_history; }

    // --- Правки (все проходят через историю) ---
    bool commit(std::string name, std::vector<SolidChange> changes);
    bool undo();
    bool redo();

    bool deleteSelection();
    // Сдвиг выделенных солидов. textureLock: текстура остаётся на месте относительно тела (компенсируется shift).
    bool translateSelection(const glm::vec3& delta, bool textureLock = true);

    // --- Выделение ---
    const std::vector<uint64_t>& selectedSolids() const { return m_selectedSolids; }
    bool isSolidSelected(uint64_t uid) const;
    uint64_t selectedFaceUid() const { return m_selectedFace; }
    uint64_t selectedFaceSolidUid() const { return m_selectedFaceSolid; }
    void clearSelection();
    void selectSolid(uint64_t uid, bool toggle);
    void selectFace(uint64_t solidUid, uint64_t faceUid);
    const Solid* findSolid(uint64_t uid) const;

    // --- Ввод ---
    // Вызывать для каждого вида каждый кадр (с over = false тоже). Наведение привязано к виду, который его
    // выставил: вид, над которым курсора нет, чужое наведение не сбрасывает (панелей несколько).
    void handlePointer(uint32_t viewId, const ViewportPointer& pointer);
    bool hasHover() const { return m_hasHover; }
    const LevelHit& hover() const { return m_hover; }

    const std::string& lastError() const { return m_lastError; }

private:
    void notify(const ChangeSet& changes);
    void pruneSelection();

    LevelDocument& m_document;
    LevelHistory m_history;
    ChangedCallback m_onChanged;

    std::vector<uint64_t> m_selectedSolids;
    uint64_t m_selectedFace = 0;
    uint64_t m_selectedFaceSolid = 0;

    LevelHit m_hover;
    bool m_hasHover = false;
    uint32_t m_hoverView = 0;

    std::string m_lastError;
};

// Копия солида, сдвинутая на delta (с компенсацией UV при textureLock)
Solid TranslatedSolid(const Solid& solid, const glm::vec3& delta, bool textureLock);
