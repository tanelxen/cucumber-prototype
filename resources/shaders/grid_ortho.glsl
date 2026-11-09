#shader vertex
#version 410 core

// Один треугольник на весь экран, без буферов: углы берутся из gl_VertexID.
// Глубина вершин 0: сетка рисуется до сцены и без записи глубины.

uniform mat4 uInvViewProj;
uniform vec3 uRight;        // оси вида в мировом пространстве (из матрицы вида)
uniform vec3 uUp;

out vec2 vPlane;            // координаты точки в плоскости вида: (вдоль right, вдоль up)

void main()
{
    vec2 ndc = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);

    // В ортографике проекция параллельная: от глубины мировая точка в плоскости вида не зависит
    vec4 world = uInvViewProj * vec4(ndc, 0.0, 1.0);
    world /= world.w;
    vPlane = vec2(dot(world.xyz, uRight), dot(world.xyz, uUp));
}

#shader fragment
#version 410 core

in vec2 vPlane;
out vec4 FragColor;

uniform float uStep;            // шаг сетки, мировые единицы
uniform float uMajorEvery;      // акцентная линия каждые N клеток
uniform float uPixelScale;      // физических пикселей на логический (retina)
uniform vec3 uAxisColorU;       // цвет линии u = 0 (она идёт вдоль up)
uniform vec3 uAxisColorV;       // цвет линии v = 0 (она идёт вдоль right)

const float kMinSpacingPx = 8.0;    // логических пикселей: мельче линии прореживаются (шаг x2, x4, ...)
const float kLineWidthPx = 1.0;
const float kAxisWidthPx = 2.0;

const vec3  kGridColor = vec3(0.85);
const float kMinorAlpha = 0.07;
const float kMajorAlpha = 0.18;
const float kAxisAlpha = 0.60;

// Покрытие пикселя линиями сетки с шагом step (1 на линии, 0 между, края сглажены).
// unitsPerPx: размер пикселя в мировых единицах; widthPx: толщина линии в физических пикселях.
float gridCoverage(vec2 p, float unitsPerPx, float step, float widthPx)
{
    vec2 cells = abs(fract(p / step - 0.5) - 0.5);          // расстояние до ближайшей линии, в клетках
    vec2 distPx = cells * (step / unitsPerPx);
    vec2 cover = clamp(0.5 * widthPx + 0.5 - distPx, 0.0, 1.0);
    return max(cover.x, cover.y);
}

vec4 over(vec4 top, vec4 bottom)
{
    float a = top.a + bottom.a * (1.0 - top.a);
    vec3 rgb = (top.rgb * top.a + bottom.rgb * bottom.a * (1.0 - top.a)) / max(a, 1e-5);
    return vec4(rgb, a);
}

void main()
{
    vec2 pixel = fwidth(vPlane);
    float unitsPerPx = max(max(pixel.x, pixel.y), 1e-6);

    float minSpacing = kMinSpacingPx * uPixelScale;
    float lineWidth = kLineWidthPx * uPixelScale;

    // Уровень детализации: шаг удваивается, пока клетка мельче minSpacing пикселей.
    // Предыдущий (более частый) уровень плавно гаснет, поэтому переходы между уровнями без скачков.
    float stepPx = uStep / unitsPerPx;
    float level = max(ceil(log2(minSpacing / stepPx)), 0.0);
    float coarse = uStep * exp2(level);
    float fine = coarse * 0.5;
    float fineFade = level > 0.0
        ? clamp((fine / unitsPerPx - 0.5 * minSpacing) / (0.5 * minSpacing), 0.0, 1.0)
        : 0.0;

    // Обычные линии. Уровни совпадают на общих линиях, поэтому берём max, а не сумму: яркость не прыгает
    float minor = max(gridCoverage(vPlane, unitsPerPx, coarse, lineWidth),
                      fineFade * gridCoverage(vPlane, unitsPerPx, fine, lineWidth));

    // Акцентные линии: каждые uMajorEvery клеток, с тем же плавным переходом
    float major = max(gridCoverage(vPlane, unitsPerPx, coarse * uMajorEvery, lineWidth),
                      fineFade * gridCoverage(vPlane, unitsPerPx, fine * uMajorEvery, lineWidth));

    vec4 color = vec4(kGridColor, max(minor * kMinorAlpha, major * kMajorAlpha));

    // Оси координат, лежащие в плоскости вида
    float axisWidth = kAxisWidthPx * uPixelScale;
    float coverU = clamp(0.5 * axisWidth + 0.5 - abs(vPlane.x) / unitsPerPx, 0.0, 1.0);
    float coverV = clamp(0.5 * axisWidth + 0.5 - abs(vPlane.y) / unitsPerPx, 0.0, 1.0);
    color = over(vec4(uAxisColorV, coverV * kAxisAlpha), color);
    color = over(vec4(uAxisColorU, coverU * kAxisAlpha), color);

    if (color.a <= 0.003) discard;
    FragColor = color;
}
