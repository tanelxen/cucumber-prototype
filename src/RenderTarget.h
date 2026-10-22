//
//  RenderTarget.h
//  Cucumber
//
//  Замена FrameBuffer. Отличия:
//   - цвет в GL_SRGB8_ALPHA8 (GL_FRAMEBUFFER_SRGB реально работает, нет бэндинга в тёмном);
//   - масштаб (retina) приходит снаружи, а не зашит в класс;
//   - не делает glGet* и не хранит сохранённый viewport: viewport выставляет bind().
//

#pragma once

class RenderTarget
{
public:
    explicit RenderTarget(int width = 1, int height = 1, float scale = 1.0f);
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    // Логический размер + масштаб. Реаллокация только при реальном изменении.
    void resize(int width, int height, float scale);
    void resize(int width, int height) { resize(width, height, m_scale); }

    void bind();    // FBO + viewport + sRGB
    void unbind();  // FBO 0, sRGB off

    int width() const { return m_width; }
    int height() const { return m_height; }
    float scale() const { return m_scale; }
    int pixelWidth() const;
    int pixelHeight() const;
    unsigned int texture() const { return m_texture; }

private:
    void create();
    void destroy();
    void reallocate();

    unsigned int m_fbo = 0;
    unsigned int m_rbo = 0;
    unsigned int m_texture = 0;

    int m_width = 1;
    int m_height = 1;
    float m_scale = 1.0f;
};
