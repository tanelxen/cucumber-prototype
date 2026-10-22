//
//  ShaderVariant.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.08.26.
//

#include "ShaderVariant.h"

#include <glad/glad.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace
{
    unsigned int CompileShader(unsigned int type, const char* source)
    {
        const unsigned int id = glCreateShader(type);

        glShaderSource(id, 1, &source, nullptr);
        glCompileShader(id);

        GLint status = GL_FALSE;
        glGetShaderiv(id, GL_COMPILE_STATUS, &status);

        if (status == GL_FALSE)
        {
            GLint length = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

            std::vector<char> message(static_cast<size_t>(length > 1 ? length : 1));
            glGetShaderInfoLog(id, static_cast<GLsizei>(message.size()), nullptr, message.data());

            printf("Failed to compile %s shader:\n%s\n",
                   (type == GL_VERTEX_SHADER) ? "vertex" : "fragment", message.data());

            glDeleteShader(id);
            return 0;
        }

        return id;
    }
}

ShaderVariant::ShaderVariant() = default;

ShaderVariant::ShaderVariant(ShaderVariant&& other) noexcept
    : m_programId(std::exchange(other.m_programId, 0u)),
      m_uniformCache(std::move(other.m_uniformCache)),
      m_locations(other.m_locations)
{
    other.m_uniformCache.clear();
    other.m_locations = Locations{};
}

ShaderVariant& ShaderVariant::operator=(ShaderVariant&& other) noexcept
{
    if (this != &other)
    {
        destroy();

        m_programId = std::exchange(other.m_programId, 0u);
        m_uniformCache = std::move(other.m_uniformCache);
        m_locations = other.m_locations;

        other.m_uniformCache.clear();
        other.m_locations = Locations{};
    }
    return *this;
}

ShaderVariant::~ShaderVariant()
{
    destroy();
}

void ShaderVariant::destroy()
{
    if (m_programId)
    {
        glDeleteProgram(m_programId);
        m_programId = 0;
    }
    m_uniformCache.clear();
    m_locations = Locations{};
}

void ShaderVariant::init(const char* vert, const char* frag)
{
    destroy();

    const unsigned int vs = CompileShader(GL_VERTEX_SHADER, vert);
    const unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, frag);

    if (vs == 0 || fs == 0)
    {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return; // программа остаётся пустой (isValid() == false), ошибка уже в логе
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDetachShader(program, vs);
    glDetachShader(program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

        std::vector<char> message(static_cast<size_t>(length > 1 ? length : 1));
        glGetProgramInfoLog(program, static_cast<GLsizei>(message.size()), nullptr, message.data());

        printf("Failed to link shader program:\n%s\n", message.data());
        glDeleteProgram(program);
        return;
    }

    m_programId = program;

    // Блок освещения -> binding point 1 (тот же индекс, что и в glBindBufferBase)
    const GLuint blockIndex = glGetUniformBlockIndex(m_programId, "LightingBlock");
    if (blockIndex != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(m_programId, blockIndex, 1);
    }

    fetchAllActiveUniforms();
    resolveLocations();
    bindSamplerUnits();
}

void ShaderVariant::bind() const
{
    glUseProgram(m_programId);
}

void ShaderVariant::unbind() const
{
    glUseProgram(0);
}

void ShaderVariant::init(const char* filepath)
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

    const std::string vertexSource = vertexStream.str();
    const std::string fragmentSource = fragmentStream.str();
    init(vertexSource.c_str(), fragmentSource.c_str());
}

int ShaderVariant::getUniformLocation(const std::string& name) const
{
    const auto it = m_uniformCache.find(name);
    return it != m_uniformCache.end() ? it->second : -1;
}

void ShaderVariant::fetchAllActiveUniforms()
{
    GLint count = 0;
    glGetProgramiv(m_programId, GL_ACTIVE_UNIFORMS, &count);

    constexpr GLsizei bufSize = 128;
    GLchar name[bufSize];

    for (GLint i = 0; i < count; ++i)
    {
        GLint size = 0;
        GLenum type = 0;
        GLsizei length = 0;
        glGetActiveUniform(m_programId, static_cast<GLuint>(i), bufSize, &length, &size, &type, name);

        const GLint location = glGetUniformLocation(m_programId, name);
        if (location != -1) m_uniformCache[name] = location; // члены uniform-блоков (location == -1) не нужны
    }
}

void ShaderVariant::resolveLocations()
{
    m_locations.model = getUniformLocation("uModel");
    m_locations.vp = getUniformLocation("uVP");
    m_locations.viewPos = getUniformLocation("uViewPos");
    m_locations.color = getUniformLocation("color");
    m_locations.bones = getUniformLocation("uBones[0]");
    m_locations.sh = getUniformLocation("u_SH[0]");
    m_locations.lightSpaceMatrices = getUniformLocation("uLightSpaceMatrices[0]");
    m_locations.shadowLightIndices = getUniformLocation("uShadowLightIndices[0]");
    m_locations.shadowLightTypes = getUniformLocation("uShadowLightTypes[0]");
    m_locations.alphaCutoff = getUniformLocation("uAlphaCutoff");
}

// Фиксированные текстурные юниты
void ShaderVariant::bindSamplerUnits()
{
    static const struct { const char* name; int unit; } kSamplers[] = {
        { "s_lightMap", 1 },
        { "s_shadowMap", 2 },
        { "s_pointShadowMap", 3 },
    };

    for (const auto& sampler : kSamplers)
    {
        const int loc = getUniformLocation(sampler.name);
        if (loc != -1) glProgramUniform1i(m_programId, loc, sampler.unit);
    }
}

// ---------------------------------------------------------------------------------------------
// По имени
// ---------------------------------------------------------------------------------------------

void ShaderVariant::setUniform(const std::string& name, const glm::vec3& vector) const
{
    setUniformAt(getUniformLocation(name), vector);
}

void ShaderVariant::setUniform(const std::string& name, const glm::vec4& vector) const
{
    setUniformAt(getUniformLocation(name), vector);
}

void ShaderVariant::setUniform(const std::string& name, const glm::mat4& matrix) const
{
    setUniformAt(getUniformLocation(name), matrix);
}

void ShaderVariant::setUniform(const std::string& name, const std::vector<glm::vec3>& vectors) const
{
    if (vectors.empty()) return;
    setUniformAt(getUniformLocation(name), vectors.data(), static_cast<int>(vectors.size()));
}

void ShaderVariant::setUniform(const std::string& name, const std::vector<glm::mat4>& matrices) const
{
    setUniformAt(getUniformLocation(name), matrices);
}

void ShaderVariant::setUniform(const std::string& name, const std::vector<int>& values) const
{
    if (values.empty()) return;
    setUniformAt(getUniformLocation(name), values.data(), static_cast<int>(values.size()));
}

void ShaderVariant::setUniform(const std::string& name, bool value) const
{
    setUniformAt(getUniformLocation(name), static_cast<int>(value));
}

void ShaderVariant::setUniform(const std::string& name, float value) const
{
    setUniformAt(getUniformLocation(name), value);
}

void ShaderVariant::setUniform(const std::string& name, int value) const
{
    setUniformAt(getUniformLocation(name), value);
}

// ---------------------------------------------------------------------------------------------
// По location
// ---------------------------------------------------------------------------------------------

void ShaderVariant::setUniformAt(int loc, const glm::vec3& value) const
{
    if (loc != -1) glUniform3fv(loc, 1, &value[0]);
}

void ShaderVariant::setUniformAt(int loc, const glm::vec4& value) const
{
    if (loc != -1) glUniform4fv(loc, 1, &value[0]);
}

void ShaderVariant::setUniformAt(int loc, const glm::mat4& value) const
{
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, &value[0][0]);
}

void ShaderVariant::setUniformAt(int loc, const glm::mat4* values, int count) const
{
    if (loc != -1 && values && count > 0) glUniformMatrix4fv(loc, count, GL_FALSE, &values[0][0][0]);
}

void ShaderVariant::setUniformAt(int loc, const glm::vec3* values, int count) const
{
    if (loc != -1 && values && count > 0) glUniform3fv(loc, count, &values[0][0]);
}

void ShaderVariant::setUniformAt(int loc, const int* values, int count) const
{
    if (loc != -1 && values && count > 0) glUniform1iv(loc, count, values);
}

void ShaderVariant::setUniformAt(int loc, const std::vector<glm::mat4>& values) const
{
    if (values.empty()) return;
    setUniformAt(loc, values.data(), static_cast<int>(values.size()));
}

void ShaderVariant::setUniformAt(int loc, float value) const
{
    if (loc != -1) glUniform1f(loc, value);
}

void ShaderVariant::setUniformAt(int loc, int value) const
{
    if (loc != -1) glUniform1i(loc, value);
}
