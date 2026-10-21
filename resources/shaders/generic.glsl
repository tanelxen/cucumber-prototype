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

#ifdef HAS_SH_LIGHT
uniform vec3 u_SH[9];
#endif

in vec3 v_Normal;

uniform vec4 color;

//final color
out vec4 FragColor;

vec3 adjustExposure(vec3 color, float value);

const float lightaxis[3] = float[](0.5f, 0.75f, 1.0f);

float getFakeLight(vec3 normal)
{
    int i;
    float f;

    // axial plane
    for ( i = 0; i < 3; i++ ) {
        if (abs(normal[i]) > 0.9) {
            f = lightaxis[i];
            return f;
        }
    }
    
    // between two axial planes
    for (i = 0; i < 3; i++) {
        if (abs(normal[i]) < 0.1) {
            f = ( lightaxis[( i + 1 ) % 3] + lightaxis[( i + 2 ) % 3] ) / 2;
            return f;
        }
    }
    
    // other
    f = ( lightaxis[0] + lightaxis[1] + lightaxis[2] ) / 3;
    return f;
}

void main()
{
    vec4 baseColor = vec4(1.0);
    
#ifdef HAS_ALBEDO_MAP
    baseColor = texture(s_baseTexture, g_TexCoord);
#endif
    
//    float gamma = 2.0;
//    FragColor.rgb = pow(FragColor.rgb, vec3(1.0/gamma));
    
    vec3 ambient = vec3(1.0);
    
#ifdef HAS_SH_LIGHT
    vec3 N = normalize(v_Normal);
    
    const float C1 = 0.282095; const float C2 = 0.488603; const float C3 = 1.092548;
    const float C4 = 0.315392; const float C5 = 0.546274;
    
    vec3 irradiance =
    C1 * u_SH[0] +
    C2 * u_SH[1] * N.y + C2 * u_SH[2] * N.z + C2 * u_SH[3] * N.x +
    C3 * u_SH[4] * (N.x * N.y) + C3 * u_SH[5] * (N.y * N.z) +
    C4 * u_SH[6] * (3.0 * N.z * N.z - 1.0) +
    C3 * u_SH[7] * (N.x * N.z) + C5 * u_SH[8] * (N.x * N.x - N.y * N.y);
    
    irradiance = max(irradiance, vec3(0.0));
    ambient = (irradiance / 3.14159265);
#endif

#ifdef HAS_LIGHT_MAP
    vec4 lightMap = texture(s_lightMap, g_TexCoord2);
    baseColor = baseColor * lightMap;

    float overbight = 10;
    baseColor.rgb = baseColor.rgb * overbight;
    //baseColor.rgb = pow(baseColor.rgb, vec3(1.0f/1.2f));
#endif

    float fakeLight = getFakeLight(v_Normal);
    baseColor.rgb = baseColor.rgb * fakeLight * 2.0f;
    
    FragColor = baseColor * color * vec4(ambient, 1);

    // simple tonemapping
    FragColor.rgb = FragColor.rgb / (FragColor.rgb + vec3(1.0f));
}

vec3 adjustExposure(vec3 color, float value) {
    return (1.0 + value) * color;
}
