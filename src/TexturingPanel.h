//
//  TexturingPanel.h
//  Cucumber
//
//  Параметры наложения текстуры на выбранные грани: смещение, масштаб, поворот, быстрые операции, флаги, буфер.
//  Выбор самой текстуры вынесен в TextureBrowserPanel (кнопка Browse открывает её).
//

#pragma once

#include "EditorPanel.h"
#include "level/FaceTexturing.h"

#include <glm/glm.hpp>
#include <string>

class TexturingPanel : public EditorPanel
{
public:
    explicit TexturingPanel(const std::string& name);

    void update(float dt) override { (void)dt; }
    void draw() override;

private:
    glm::vec2 m_repeat{1.0f, 1.0f};
    float m_nudgeStep = 0.0f;              // 0: шаг сетки документа

    FaceTexturing::FaceClipboard m_clipboard;
    bool m_pasteMaterial = true;
    bool m_pasteUV = true;
    bool m_pasteWrap = true;               // true: продолжать рисунок через ребро; false: та же мировая проекция
};
