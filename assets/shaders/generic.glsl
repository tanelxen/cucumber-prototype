#shader vertex
#version 410 core

uniform mat4 MVP;
layout (location = 0) in vec4 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

out vec2 g_TexCoord;

void main()
{
    gl_Position = MVP * position;
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
    
//    float gamma = 2.0;
//    FragColor.rgb = pow(FragColor.rgb, vec3(1.0/gamma));
}
