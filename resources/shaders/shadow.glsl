#shader vertex

layout (location = 0) in vec4 position;

#ifdef HAS_SKINNING
layout (location = 3) in ivec4 boneIndices;
layout (location = 4) in vec4 boneWeights;
uniform mat4 uBones[128];
#endif

uniform mat4 uModel;
uniform mat4 uVP;
out vec3 vWorldPosition;

void main()
{
    vec4 localPosition = position;

#ifdef HAS_SKINNING
    mat4 boneTransform = uBones[boneIndices[0]] * boneWeights[0];
    boneTransform += uBones[boneIndices[1]] * boneWeights[1];
    boneTransform += uBones[boneIndices[2]] * boneWeights[2];
    boneTransform += uBones[boneIndices[3]] * boneWeights[3];
    localPosition = boneTransform * localPosition;
#endif

    vec4 worldPosition = uModel * localPosition;
    vWorldPosition = worldPosition.xyz;
    gl_Position = uVP * worldPosition;
}

#shader fragment

in vec3 vWorldPosition;
uniform bool uRadialDepth;
uniform vec3 uLightPosition;
uniform float uFarPlane;

void main()
{
    gl_FragDepth = gl_FragCoord.z;
    if (uRadialDepth)
    {
        gl_FragDepth = length(vWorldPosition - uLightPosition) / uFarPlane;
    }
}
