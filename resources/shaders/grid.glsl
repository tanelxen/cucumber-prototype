#shader vertex
#version 410 core

// Один треугольник на весь экран, без буферов: углы берутся из gl_VertexID.
// Глубина вершин 0: сетка рисуется до сцены и без записи глубины.
//
// Два режима:
//   ортографика: плоскость сетки - плоскость вида, координаты берутся по осям uRight / uUp;
//   перспектива: плоскость y = 0, точка находится пересечением луча через пиксель с плоскостью.
//   Оси uRight / uUp при этом (1,0,0) и (0,0,1): так же задаёт плоскость сетки и ортографика.

uniform mat4 uInvViewProj;
uniform vec3 uRight;
uniform vec3 uUp;

out vec2 vPlane;    // ортографика: координаты точки в плоскости вида
out vec3 vNear;     // перспектива: луч через пиксель (точки на ближней и дальней плоскостях).
out vec3 vFar;      // Обе плоскости проецируются аффинно, поэтому линейная интерполяция по экрану точна.

vec3 unproject(vec2 ndc, float z)
{
    vec4 world = uInvViewProj * vec4(ndc, z, 1.0);
    return world.xyz / world.w;
}

void main()
{
    vec2 ndc = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);

    vNear = unproject(ndc, -1.0);
    vFar = unproject(ndc, 1.0);

    // В ортографике проекция параллельная: от глубины точка в плоскости вида не зависит
    vec3 onViewPlane = unproject(ndc, 0.0);
    vPlane = vec2(dot(onViewPlane, uRight), dot(onViewPlane, uUp));
}

#shader fragment
#version 410 core

in vec2 vPlane;
in vec3 vNear;
in vec3 vFar;
out vec4 FragColor;

uniform bool uPerspective;
uniform vec3 uCameraPos;
uniform vec3 uRight;
uniform vec3 uUp;

uniform float uStep;            // шаг сетки, мировые единицы
uniform float uMajorEvery;      // акцентная линия каждые N клеток
uniform float uPixelScale;      // физических пикселей на логический (retina)
uniform vec3 uAxisColorU;       // цвет линии u = 0 (она идёт вдоль uUp)
uniform vec3 uAxisColorV;       // цвет линии v = 0 (она идёт вдоль uRight)

const float kMinSpacingPx = 8.0;    // логических пикселей: мельче линии прореживаются (шаг x2, x4, ...)
const float kLineWidthPx = 1.0;
const float kAxisWidthPx = 2.0;

const vec3  kGridColor = vec3(0.85);
const float kMinorAlpha = 0.07;
const float kMajorAlpha = 0.18;
const float kAxisAlpha = 0.60;

// Перспектива: сетка тает с расстоянием и у горизонта (там клетки в пиксель и мельче, возникает муар)
const float kFadeStart = 2048.0;
const float kFadeEnd = 8192.0;
const float kHorizonFade = 0.08;    // модуль sin угла луча к плоскости, ниже которого сетка гаснет

// Покрытие пикселя линиями сетки с шагом step (1 на линии, 0 между, края сглажены).
// unitsPerPx: размер пикселя в мировых единицах по каждой оси сетки; widthPx: толщина линии в физических пикселях.
float gridCoverage(vec2 p, vec2 unitsPerPx, float step, float widthPx)
{
    vec2 cells = abs(fract(p / step - 0.5) - 0.5);          // расстояние до ближайшей линии, в клетках
    vec2 distPx = cells * step / unitsPerPx;
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
    // Точка на плоскости сетки и коэффициент затухания
    vec2 plane = vPlane;
    float visibility = 1.0;
    bool hitsPlane = true;

    if (uPerspective)   // uniform: ветвление не мешает производным ниже
    {
        vec3 dir = vFar - vNear;
        float t = -vNear.y / dir.y;                 // пересечение с плоскостью y = 0
        vec3 hit = vNear + dir * t;
        plane = vec2(dot(hit, uRight), dot(hit, uUp));

        hitsPlane = t > 0.0;                        // NaN (луч параллелен плоскости) даёт false
        float grazing = abs(normalize(dir).y);
        visibility = (1.0 - smoothstep(kFadeStart, kFadeEnd, length(hit - uCameraPos)))
                   * smoothstep(0.0, kHorizonFade, grazing);
    }

    // Производные берём до любого discard
    vec2 unitsPerPx = max(fwidth(plane), vec2(1e-6));
    if (!hitsPlane) discard;

    float upp = max(unitsPerPx.x, unitsPerPx.y);
    float minSpacing = kMinSpacingPx * uPixelScale;
    float lineWidth = kLineWidthPx * uPixelScale;

    // Уровень детализации: шаг удваивается, пока клетка мельче minSpacing пикселей.
    // Предыдущий (более частый) уровень плавно гаснет, поэтому переходы между уровнями без скачков.
    float stepPx = uStep / upp;
    float level = max(ceil(log2(minSpacing / stepPx)), 0.0);
    float coarse = uStep * exp2(level);
    float fine = coarse * 0.5;
    float fineFade = level > 0.0
        ? clamp((fine / upp - 0.5 * minSpacing) / (0.5 * minSpacing), 0.0, 1.0)
        : 0.0;

    // Обычные линии. Уровни совпадают на общих линиях, поэтому берём max, а не сумму: яркость не прыгает
    float minor = max(gridCoverage(plane, unitsPerPx, coarse, lineWidth),
                      fineFade * gridCoverage(plane, unitsPerPx, fine, lineWidth));

    // Акцентные линии: каждые uMajorEvery клеток, с тем же плавным переходом
    float major = max(gridCoverage(plane, unitsPerPx, coarse * uMajorEvery, lineWidth),
                      fineFade * gridCoverage(plane, unitsPerPx, fine * uMajorEvery, lineWidth));

    vec4 color = vec4(kGridColor, max(minor * kMinorAlpha, major * kMajorAlpha));

    // Оси координат, лежащие в плоскости сетки
    float axisWidth = kAxisWidthPx * uPixelScale;
    float coverU = clamp(0.5 * axisWidth + 0.5 - abs(plane.x) / unitsPerPx.x, 0.0, 1.0);
    float coverV = clamp(0.5 * axisWidth + 0.5 - abs(plane.y) / unitsPerPx.y, 0.0, 1.0);
    color = over(vec4(uAxisColorV, coverV * kAxisAlpha), color);
    color = over(vec4(uAxisColorU, coverU * kAxisAlpha), color);

    color.a *= visibility;
    if (color.a <= 0.003) discard;
    FragColor = color;
}
