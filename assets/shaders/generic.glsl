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

uniform mat4 MVP;

void main()
{
    vec4 localPosition = position;
    
#ifdef HAS_SKINNING
    mat4 boneTransform = uBones[boneIndices[0]] * boneWights[0];
    boneTransform += uBones[boneIndices[1]] * boneWights[1];
    boneTransform += uBones[boneIndices[2]] * boneWights[2];
    boneTransform += uBones[boneIndices[3]] * boneWights[3];
    
    localPosition = boneTransform * localPosition;
#endif
    
#ifdef HAS_ALBEDO_MAP
    g_TexCoord = texCoord;
#endif
    
    gl_Position = MVP * localPosition;
}

#shader fragment

#ifdef HAS_ALBEDO_MAP
in vec2 g_TexCoord;
uniform sampler2D s_baseTexture;
#endif

uniform vec4 color;

//final color
out vec4 FragColor;

void main()
{
    vec4 baseColor = vec4(1.0);
    
#ifdef HAS_ALBEDO_MAP
    baseColor = texture(s_baseTexture, g_TexCoord);
#endif
    
    FragColor = baseColor * color;
    
//    float gamma = 2.0;
//    FragColor.rgb = pow(FragColor.rgb, vec3(1.0/gamma));
}
