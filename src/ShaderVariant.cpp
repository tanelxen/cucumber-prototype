//
//  ShaderVariant.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.08.26.
//

#include "ShaderVariant.h"
#include <glad/glad.h>
#include <cstdio>

#include <iostream>
#include <fstream>
#include <sstream>

ShaderVariant::ShaderVariant()
{
    m_programId = 0;
}

ShaderVariant::ShaderVariant(ShaderVariant &&other) noexcept
{
    m_programId = other.m_programId;
    other.m_programId = 0;
}

ShaderVariant &ShaderVariant::operator=(ShaderVariant &&other) noexcept
{
    m_programId = other.m_programId;
    other.m_programId = 0;

    return *this;
}

ShaderVariant::~ShaderVariant()
{
    glDeleteShader(m_programId);
}

static unsigned int compile_shader(unsigned int type, const char* source);

void ShaderVariant::init(const char *vert, const char *frag)
{
    m_programId = glCreateProgram();

    unsigned int vs = compile_shader(GL_VERTEX_SHADER, vert);
    unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, frag);

    glAttachShader(m_programId, vs);
    glAttachShader(m_programId, fs);
    glLinkProgram(m_programId);
    glValidateProgram(m_programId);

    glDeleteShader(vs);
    glDeleteShader(fs);
    
    fetchAllActiveUniforms();
}

void ShaderVariant::bind() const
{
    glUseProgram(m_programId);
}

void ShaderVariant::unbind() const
{
    glUseProgram(0);
}

void ShaderVariant::init(const char *filepath)
{
    enum ShaderSourceType {
        SRC_NONE = -1,
        SRC_VERTEX = 0,
        SCR_FRAGMENT = 1
    };

    std::ifstream stream(filepath);
    
    if (!stream.is_open())
    {
        std::cerr << "Error: Failed to open the file at path: " << filepath << std::endl;
        return;
    }

    std::string line;
    std::stringstream vertexStream;
    std::stringstream fragmentStream;

    ShaderSourceType currentType = SRC_NONE;

    while (getline(stream, line))
    {
        if (line.find("#shader") != std::string::npos)
        {
            if (line.find("vertex") != std::string::npos)
            {
                currentType = SRC_VERTEX;

            }
            else if (line.find("fragment") != std::string::npos)
            {
                currentType = SCR_FRAGMENT;
            }
        }
        else if (currentType == SRC_VERTEX)
        {
            vertexStream << line << "\n";
        }
        else if (currentType == SCR_FRAGMENT)
        {
            fragmentStream << line << "\n";
        }
    }

    init(vertexStream.str().c_str(), fragmentStream.str().c_str());
}

void ShaderVariant::setUniform(const std::string& name, const glm::vec3& vector) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, 1, (const float*) &vector);
    }
}

void ShaderVariant::setUniform(const std::string& name, const glm::vec4& vector) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4fv(location, 1, (const float*) &vector);
    }
}

void ShaderVariant::setUniform(const std::string &name, const glm::mat4& matrix) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, (const float*) &matrix);
    }
}

void ShaderVariant::setUniform(const std::string &name, const std::vector<glm::vec3> &vectors) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, (GLsizei)(vectors.size()), &(vectors[0][0]));
    }
}

void ShaderVariant::setUniform(const std::string &name, const std::vector<glm::mat4> &matrices) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, (GLsizei)(matrices.size()), GL_FALSE, &(matrices[0][0][0]));
    }
}

void ShaderVariant::setUniform(const std::string &name, bool value) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void ShaderVariant::setUniform(const std::string &name, float value) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void ShaderVariant::setUniform(const std::string &name, int value) const
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

int ShaderVariant::getUniformLocation(const std::string& name) const
{
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) {
        return it->second;
    }
    return -1;
}

void ShaderVariant::fetchAllActiveUniforms()
{
    GLint count;
    glGetProgramiv(m_programId, GL_ACTIVE_UNIFORMS, &count);
    
    GLint size;
    GLenum type;
    const GLsizei bufSize = 64;
    GLchar name[bufSize];
    GLsizei length;
    
    for (GLint i = 0; i < count; i++)
    {
        glGetActiveUniform(m_programId, (GLuint)i, bufSize, &length, &size, &type, name);
        GLint location = glGetUniformLocation(m_programId, name);
        m_uniformCache[name] = location;
    }
}

static unsigned int compile_shader(unsigned int type, const char* source)
{
    unsigned int id = glCreateShader(type);

    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);

    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

        char message[1024];
        glGetShaderInfoLog(id, length, &length, message);

        printf("Failed to compile %s shader:\n", (type == GL_VERTEX_SHADER) ? "vertex" : "fragment");
        printf("%s\n", message);
        return 0;
    }

    return id;
}
