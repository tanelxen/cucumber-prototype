#shader vertex
#version 410 core
layout (location = 0) in vec4 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in ivec4 boneIndices;
layout (location = 4) in vec4 boneWights;

uniform mat4 uBones[128];
uniform mat4 MVP;

out vec2 g_TexCoord;

void main()
{
    mat4 BoneTransform = uBones[boneIndices[0]] * boneWights[0];
    BoneTransform += uBones[boneIndices[1]] * boneWights[1];
    BoneTransform += uBones[boneIndices[2]] * boneWights[2];
    BoneTransform += uBones[boneIndices[3]] * boneWights[3];
    
    gl_Position = MVP * BoneTransform * position;
    g_TexCoord = vec2(texCoord.x, texCoord.y);
}

#shader fragment
#version 410 core

in vec2 g_TexCoord;

uniform sampler2D s_baseTexture;
uniform vec4 color;

//final color
out vec4 FragColor;

void main()
{
    vec4 o_texture  = texture(s_baseTexture, g_TexCoord);
    FragColor = o_texture * color;
}
