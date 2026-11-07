#shader vertex

layout (location = 0) in vec4 position;
layout (location = 1) in vec3 normal;

#ifdef HAS_ALBEDO_MAP
layout (location = 2) in vec2 texCoord;
out vec2 g_TexCoord;
#endif

#ifdef HAS_SKINNING
layout (location = 3) in ivec4 boneIndices;
layout (location = 4) in vec4 boneWights;

uniform mat4 uBones[128];
#endif

#ifdef HAS_LIGHT_MAP
layout (location = 5) in vec2 texCoord2;
out vec2 g_TexCoord2;
#endif

out vec3 vFragPos;
out vec3 v_Normal;

//uniform mat4 MVP;

uniform mat4 uModel;
uniform mat4 uVP;

void main()
{
    vec4 localPosition = position;
    v_Normal = normalize(mat3(uModel) * normal);
    
#ifdef HAS_SKINNING
    mat4 boneTransform = uBones[boneIndices[0]] * boneWights[0];
    boneTransform += uBones[boneIndices[1]] * boneWights[1];
    boneTransform += uBones[boneIndices[2]] * boneWights[2];
    boneTransform += uBones[boneIndices[3]] * boneWights[3];
    
    localPosition = boneTransform * localPosition;
    
    v_Normal = normalize(mat3(uModel * boneTransform) * normal);
#endif

    vFragPos = (uModel * localPosition).xyz;
    
#ifdef HAS_ALBEDO_MAP
    g_TexCoord = texCoord;
#endif

#ifdef HAS_LIGHT_MAP
    g_TexCoord2 = texCoord2;
#endif
    
    gl_Position = uVP * uModel * localPosition;
}

#shader fragment

#ifdef HAS_ALBEDO_MAP
in vec2 g_TexCoord;
uniform sampler2D s_baseTexture;
#endif

#ifdef HAS_LIGHT_MAP
in vec2 g_TexCoord2;
uniform sampler2D s_lightMap;
#endif

#ifdef HAS_SHADOWS
uniform mat4 uLightSpaceMatrices[4];
uniform sampler2DArray s_shadowMap;
uniform samplerCubeArray s_pointShadowMap;
#endif

#ifdef HAS_SH_LIGHT
uniform vec3 u_SH[9];
#endif

in vec3 vFragPos;
in vec3 v_Normal;

uniform vec4 color;
uniform vec3 uViewPos;

uniform float uAlphaCutoff;

struct LightData {
    vec4 position;   // xyz - pos, w - type (0=directional, 1=point, 2=spot)
    vec4 direction;  // xyz - dir,  w - range (в Quake-юнитах, например 500.0)
    vec4 color;      // xyz - color, w - intensity
    vec4 parameters; // x - spot outer angle cosine
};

layout (std140) uniform LightingBlock {
    LightData uLights[16];
    int uLightCount;
};

out vec4 FragColor;

vec3 adjustExposure(vec3 color, float value);

const float lightaxis[3] = float[](0.5f, 0.75f, 1.0f);

float getFakeLight(vec3 normal)
{
    int i;
    float f;
    for ( i = 0; i < 3; i++ ) {
        if (abs(normal[i]) > 0.9) {
            f = lightaxis[i];
            return f;
        }
    }
    for (i = 0; i < 3; i++) {
        if (abs(normal[i]) < 0.1) {
            f = ( lightaxis[( i + 1 ) % 3] + lightaxis[( i + 2 ) % 3] ) / 2;
            return f;
        }
    }
    f = ( lightaxis[0] + lightaxis[1] + lightaxis[2] ) / 3;
    return f;
}

#ifdef HAS_SHADOWS

float getScreenNoise() {
    // Высокочастотный шум Interleaved Gradient Noise (используется в современных движках)
    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.0605, 0.00598))));
}

float getShadowVisibility(vec3 normal, int lightIndex, int shadowMapIndex, int shadowType)
{
    // Масштаб смещения для Quake-юнитов (базовый шаг ~2-5 юнитов).
    // Если тени начнут сильно отрываться от объектов, уменьшите это число (например, до 1.0)
    float quakeScaleFactor = 1.5; 

    // === 1. POINT LIGHTS (Радиальные тени из Cubemap с дизерингом) ===
    if (shadowType == 1)
    {
        vec3 lightToFragment = vFragPos - uLights[lightIndex].position.xyz;
        vec3 lightDirection = normalize(lightToFragment);
        
        // Возвращаем нормальный, небольшой Quake-сдвиг, так как дизеринг скроет угри
        float ndotl = max(dot(normal, -lightDirection), 0.0);
        float offsetScale = clamp(1.0 - ndotl, 0.0, 1.0);
        
        // Уменьшите коэффицент (например, до 0.5 - 1.0), чтобы тени не "отрывались"
        vec3 biasedPos = vFragPos + normal * offsetScale * 1.0; 
        
        vec3 biasedLightToFragment = biasedPos - uLights[lightIndex].position.xyz;
        float range = max(uLights[lightIndex].direction.w, 0.0001);
        float fragmentDepth = length(biasedLightToFragment) / range;

        // Строим локальный базис вокруг луча света
        vec3 helper = abs(lightDirection.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(lightDirection, helper));
        vec3 bitangent = cross(lightDirection, tangent);
        
        // Тексельный шаг для кубической карты
        float shadowResolution = float(textureSize(s_pointShadowMap, 0).x);
        float angularTexelSize = 2.0 / shadowResolution;
        
        // Получаем случайный угол из координат экрана
        float randomAngle = getScreenNoise() * 6.2831853;
        vec2 rotateVector = vec2(cos(randomAngle), sin(randomAngle));
        
        // Диск Пуассона для 3D-сэмплинга кубической карты
        vec2 poissonDisk[4] = vec2[](
            vec2(-0.94201624, -0.39906216),
            vec2(0.94558609, -0.76890725),
            vec2(-0.094184101, -0.92938870),
            vec2(0.34495938, 0.29387760)
        );
        
        // Базовый bias для радиальной глубины точечного источника
        float bias = max(0.002, 0.006 * (1.0 - ndotl));
        float visibleSamples = 0.0;
        
        // Радиус фильтрации (чем больше, тем мягче тень, но сильнее виден шум)
        float filterRadius = 2.0; 

        for (int i = 0; i < 4; i++)
        {
            // Случайный поворот 2D-сэмпла диска
            vec2 rotatedOffset = vec2(
                poissonDisk[i].x * rotateVector.x - poissonDisk[i].y * rotateVector.y,
                poissonDisk[i].x * rotateVector.y + poissonDisk[i].y * rotateVector.x
            );
            
            // Переносим повернутый сдвиг в 3D-пространство через тангент-базис луча света
            vec3 sampleOffset = (tangent * rotatedOffset.x + bitangent * rotatedOffset.y) * angularTexelSize * filterRadius;
            vec3 sampleDirection = normalize(lightDirection + sampleOffset);
            
            float closestDepth = texture(
                s_pointShadowMap, vec4(sampleDirection, float(shadowMapIndex))).r;
                
            visibleSamples += (fragmentDepth - bias <= closestDepth) ? 1.0 : 0.0;
        }
        
        return visibleSamples / 4.0;
    }

    // === 2. SPOT & DIRECTIONAL LIGHTS (2D Текстурные карты теней) ===
    vec3 L = (shadowType == 2) 
        ? normalize(uLights[lightIndex].position.xyz - vFragPos) 
        : normalize(-uLights[lightIndex].direction.xyz);
        
    float ndotl = max(dot(normal, L), 0.0);
    float offsetScale = clamp(1.0 - ndotl, 0.0, 1.0);
    
    // Смещаем мировую позицию фрагмента ДО умножения на матрицу LightSpace
    vec3 biasedFragPos = vFragPos + normal * offsetScale * quakeScaleFactor;

    vec4 lightSpacePosition = uLightSpaceMatrices[shadowMapIndex] * vec4(biasedFragPos, 1.0);
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;

    if (projected.z < 0.0 || projected.z > 1.0 ||
        projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0)
    {
        return 1.0; // Вне усеченного конуса тени — считаем освещенным
    }

    // ТЕПЕРЬ И ДЛЯ SPOT, И ДЛЯ DIRECTIONAL берем нелинейную глубину из проекции!
    float visibilityDepth = projected.z;
    float bias;
    
    if (shadowType == 2)
    {
        // Наклонный bias для нелинейной глубины Spot-лайта
        bias = max(0.001 * (1.0 - ndotl), 0.0001);
    }
    else
    {
        // Наклонный bias для Directional света
        bias = max(0.0005 * (1.0 - ndotl), 0.00005);
    }

    vec2 texelSize = 1.0 / vec2(textureSize(s_shadowMap, 0).xy);
    float visibleSamples = 0.0;
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(
                s_shadowMap,
                vec3(projected.xy + vec2(x, y) * texelSize, float(shadowMapIndex))).r;
            visibleSamples += (visibilityDepth - bias <= closestDepth) ? 1.0 : 0.0;
        }
    }
    return visibleSamples / 9.0;
}
#endif

void main()
{
    vec4 baseColor = vec4(1.0);
    
#ifdef HAS_ALBEDO_MAP
    baseColor = texture(s_baseTexture, g_TexCoord);
#endif
    
    if (baseColor.a < uAlphaCutoff) discard;

    vec3 diffuseTotal = vec3(0.0);
    vec3 albedo = baseColor.rgb;
    vec3 baked = vec3(0.0);

#ifdef HAS_LIGHT_MAP
    baked = texture(s_lightMap, g_TexCoord2).rgb;
#else
    diffuseTotal = vec3(0.1);
// #else
//     float fakeLight = getFakeLight(v_Normal);
//     albedo *= fakeLight * 2.0;
#endif



    for(int i = 0; i < uLightCount; i++)
    {
#ifdef HAS_LIGHT_MAP
        // BakedAndRealtime: на лайтмапленных поверхностях такой источник уже запечён
        // parameters.z = 1 для LightMode::BakedAndRealtime
        if (uLights[i].parameters.z > 0.5) continue;
#endif

        int type = int(uLights[i].position.w);
        vec3 lightColor = uLights[i].color.rgb * uLights[i].color.w;
        
        vec3 lightDir;
        float attenuation = 1.0;

        if(type == 0) // Directional Light
        {
            lightDir = normalize(-uLights[i].direction.xyz);
        }
        else // Point or Spot light
        {
            vec3 lightToFrag = uLights[i].position.xyz - vFragPos;
            float distance = length(lightToFrag);
            lightDir = normalize(lightToFrag);
            
            float range = max(uLights[i].direction.w, 0.0001);
            
            // --- РЕШЕНИЕ ПРОБЛЕМЫ КРАЕВ РАДИУСА (Epic Attenuation) ---
            // Вместо линейного падения делаем гладкое затухание 4-й степени, 
            // которое гарантированно сводит интенсивность и ступенчатость PCF к 0 на границе range.
            float ratio = clamp(distance / range, 0.0, 1.0);
            float ratio2 = ratio * ratio;
            float ratio4 = ratio2 * ratio2;
            attenuation = clamp(1.0 - ratio4, 0.0, 1.0);
            attenuation *= attenuation; // Квадратичный спад для естественной мягкости лампы

            if (type == 2) // Spot Light
            {
                float coneCosine = dot(normalize(vFragPos - uLights[i].position.xyz),
                                       normalize(uLights[i].direction.xyz));
                float innerCosine = cos(acos(clamp(uLights[i].parameters.x, -1.0, 1.0)) * 0.8);
                attenuation *= smoothstep(uLights[i].parameters.x, innerCosine, coneCosine);
            }
        }

        // ранний выход до расчёта тени: источники вне радиуса и за спиной поверхности её не считают
        float diff = max(dot(v_Normal, lightDir), 0.0);
        if (diff * attenuation <= 0.0) continue;

        float visibility = 1.0;
        
#ifdef HAS_SHADOWS
        int shadowSlot = int(uLights[i].parameters.y);   // -1 = без тени
        if (shadowSlot >= 0)
        {
            visibility = getShadowVisibility(normalize(v_Normal), i, shadowSlot, type);
        }
#endif
        diffuseTotal += diff * lightColor * attenuation * visibility;
    }
    
    vec3 lighting = baked + diffuseTotal;
    vec3 finalColor = baseColor.rgb * lighting;

    FragColor = vec4(finalColor, baseColor.a);

    // Tone mapping
    FragColor.rgb = FragColor.rgb / (FragColor.rgb + vec3(1.0f));
}

vec3 adjustExposure(vec3 color, float value) {
    return (1.0 + value) * color;
}