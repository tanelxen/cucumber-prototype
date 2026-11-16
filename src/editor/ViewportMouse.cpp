//
//  ViewportMouse.cpp
//  Cucumber
//

#include "ViewportMouse.h"
#include "core/Input.h"
#include <cmath>

ViewportMouseOwnership::~ViewportMouseOwnership()
{
    if (s_owner == this)
    {
        s_owner = nullptr;
    }
}

void ViewportMouseOwnership::sync(bool cameraCapturing)
{
    if (cameraCapturing && !s_owner)             s_owner = this;
    else if (!cameraCapturing && s_owner == this) s_owner = nullptr;

    if (cameraCapturing == m_cursorCaptured) return;

    m_cursorCaptured = cameraCapturing;
    Input::setCursorEnabled(!cameraCapturing);
}

void RightMouseGesture::press(bool altHeld)
{
    m_tracking = true;
    m_navigating = false;
    m_travel = 0.0f;
    m_alt = altHeld;
}

void RightMouseGesture::accumulate(float offsetX, float offsetY)
{
    if (!m_tracking || m_navigating) return;

    m_travel += std::sqrt(offsetX * offsetX + offsetY * offsetY);
    m_navigating = m_travel >= kNavigationThreshold;
}

RightMouseGesture::Release RightMouseGesture::release(bool clickAllowed)
{
    const bool click = m_tracking && !m_navigating && clickAllowed;
    const bool alt = m_alt;
    cancel();

    if (!click) return Release::None;
    return alt ? Release::AltClick : Release::Click;
}

void RightMouseGesture::cancel()
{
    m_tracking = false;
    m_navigating = false;
}
