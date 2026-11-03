//
//  TexturingPanel.h
//  Cucumber
//
//  Панель текстурирования граней уровня. Работает только в режиме правки геометрии (MapComponent::editSession()).
//  Цель правок: выбранные грани (Alt+клик, Ctrl+Alt+клик добавляет), а если граней нет, все грани выбранных солидов.
//  Все правки идут через LevelEditSession (undo / redo); перетаскивание полей сливается в одну запись undo.
//

#pragma once

#include "EditorPanel.h"
#include "level/FaceTexturing.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class TexturingPanel : public EditorPanel
{
public:
    explicit TexturingPanel(const std::string& name);

    void update(float dt) override { (void)dt; }
    void draw() override;

private:
    struct TextureInfo
    {
        int width = 256;       // как в MapMesh: нет файла, считаем 256 x 256
        int height = 256;
        unsigned int handle = 0;
    };

    const TextureInfo& texture(const std::string& name);
    void refreshCatalog();
    bool materialMatchesFilter(const std::string& name) const;
    // Клик по материалу кладёт имя в m_pendingMaterial; применяется в конце кадра
    void drawMaterialGrid(const char* id, const std::vector<std::string>& names, const std::string& current, float height);

    // Каталог текстур на диске (wolf/textures, без расширения); обновляется кнопкой Refresh
    std::vector<std::string> m_catalog;
    bool m_catalogLoaded = false;

    std::unordered_map<std::string, TextureInfo> m_textures;
    std::string m_pendingMaterial;

    char m_filter[128] = "";
    float m_thumbnailSize = 72.0f;

    glm::vec2 m_repeat{1.0f, 1.0f};
    float m_nudgeStep = 0.0f;              // 0: шаг сетки документа

    FaceTexturing::FaceClipboard m_clipboard;
    bool m_pasteMaterial = true;
    bool m_pasteUV = true;
};
