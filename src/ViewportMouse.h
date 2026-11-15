//
//  ViewportMouse.h
//  Cucumber
//
//  Два небольших куска состояния мыши, которые раньше были россыпью полей ViewportPanel.
//
//  ViewportMouseOwnership: единственная панель, которой принадлежит мышь (летит камера, курсор скрыт),
//  плюс синхронизация курсора. Заменяет static s_mouseOwner, syncCursor и updateOwnership.
//
//  RightMouseGesture: жест ПКМ "клик или навигация". Если мышь сдвинулась меньше порога, это клик
//  (меню создания / Alt: перенос текстуры), иначе камера летит. Не зависит от ImGui и от сцены.
//

#pragma once

class ViewportMouseOwnership
{
public:
    ViewportMouseOwnership() = default;
    ~ViewportMouseOwnership();

    ViewportMouseOwnership(const ViewportMouseOwnership&) = delete;
    ViewportMouseOwnership& operator=(const ViewportMouseOwnership&) = delete;

    // Мышью не владеет ни одна панель
    static bool IsFree() { return s_owner == nullptr; }

    bool owns() const { return s_owner == this; }

    // Приводит владение и курсор в соответствие с камерой. Курсором управляем только по смене состояния:
    // иначе несколько панелей и игра дерутся за него.
    void sync(bool cameraCapturing);

private:
    static inline ViewportMouseOwnership* s_owner = nullptr;

    bool m_cursorCaptured = false;
};

class RightMouseGesture
{
public:
    enum class Release { None, Click, AltClick };

    // ПКМ нажата над видом
    void press(bool altHeld);

    // Каждый кадр, пока ПКМ зажата и навигация ещё не началась: накапливает перемещение мыши
    void accumulate(float offsetX, float offsetY);

    // ПКМ отпущена. clickAllowed = false (например, идёт симуляция): клика не будет.
    // Сбрасывает жест.
    Release release(bool clickAllowed);

    void cancel();

    bool tracking() const { return m_tracking; }
    bool navigating() const { return m_navigating; }

    static constexpr float kNavigationThreshold = 5.0f;   // пикселей

private:
    bool m_tracking = false;
    bool m_navigating = false;
    bool m_alt = false;
    float m_travel = 0.0f;
};
