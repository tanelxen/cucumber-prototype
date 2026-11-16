//
//  LevelEditState.h
//  Cucumber
//
//  Всё, что существует, пока уровень в режиме правки: сессия редактора (инструменты, история) и меш на GPU
//  с инкрементальными обновлениями. Раньше это были три поля MapComponent (m_edit, m_editMesh, m_editChanged),
//  связанные через колбэк.
//

#pragma once

#include "level/LevelDocument.h"

#include <memory>

class LevelEditSession;
class LevelEditMesh;

class LevelEditState
{
public:
    // level должен пережить это состояние (сессия хранит ссылку; MapComponent объявляет level раньше)
    explicit LevelEditState(LevelDocument& level);
    ~LevelEditState();

    // Колбэк сессии захватывает this
    LevelEditState(const LevelEditState&) = delete;
    LevelEditState& operator=(const LevelEditState&) = delete;

    LevelEditSession& session() { return *m_session; }
    LevelEditMesh& mesh() { return *m_mesh; }

    // Геометрия менялась с момента создания: при выходе из правки нужно пересобрать запечённый меш и физику
    bool changed() const { return m_changed; }

private:
    bool m_changed = false;
    std::unique_ptr<LevelEditMesh> m_mesh;
    std::unique_ptr<LevelEditSession> m_session;   // последним: его колбэк использует меш (уничтожается первым)
};
