#shader vertex
#version 410 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    mat4 staticView = mat4(mat3(view));
    vec4 pos = projection * staticView * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}

#shader fragment
#version 410 core

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    vec3 envColor = texture(skybox, TexCoords).rgb;
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0 / 2.2));
    
    FragColor = vec4(envColor, 1.0);
    
//    FragColor = vec4(1, 1, 0, 1);
}
