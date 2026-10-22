//
//  ShaderVariant.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.08.26.
//

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

class ShaderVariant
{
public:
    // Часто используемые uniform'ы: ищутся один раз при линковке. -1 = в шейдере нет.
    struct Locations
    {
        int model = -1;
        int vp = -1;
        int viewPos = -1;
        int color = -1;
        int bones = -1;
        int sh = -1;
        int lightSpaceMatrices = -1;
        int shadowLightIndices = -1;
        int shadowLightTypes = -1;
        int alphaCutoff = -1;
    };

    ShaderVariant();
    ~ShaderVariant();

    ShaderVariant(const ShaderVariant&) = delete;
    ShaderVariant& operator=(const ShaderVariant&) = delete;
    ShaderVariant(ShaderVariant&& other) noexcept;
    ShaderVariant& operator=(ShaderVariant&& other) noexcept;

    void init(const char* vert, const char* frag);
    void init(const char* filepath);

    bool isValid() const { return m_programId != 0; }
    void bind() const;
    void unbind() const;
    unsigned int getProgramId() const { return m_programId; }

    const Locations& locations() const { return m_locations; }
    int getUniformLocation(const std::string& name) const;

    // --- По имени: холодный путь (инициализация, редкие вызовы) ---
    void setUniform(const std::string& name, const glm::vec3& vector) const;
    void setUniform(const std::string& name, const glm::vec4& vector) const;
    void setUniform(const std::string& name, const glm::mat4& matrix) const;
    void setUniform(const std::string& name, const std::vector<glm::vec3>& vectors) const;
    void setUniform(const std::string& name, const std::vector<glm::mat4>& matrices) const;
    void setUniform(const std::string& name, const std::vector<int>& values) const;
    void setUniform(const std::string& name, bool value) const;
    void setUniform(const std::string& name, float value) const;
    void setUniform(const std::string& name, int value) const;

    // --- По location: горячий путь. loc == -1 игнорируется ---
    void setUniformAt(int loc, const glm::vec3& value) const;
    void setUniformAt(int loc, const glm::vec4& value) const;
    void setUniformAt(int loc, const glm::mat4& value) const;
    void setUniformAt(int loc, const glm::mat4* values, int count) const;
    void setUniformAt(int loc, const glm::vec3* values, int count) const;
    void setUniformAt(int loc, const int* values, int count) const;
    void setUniformAt(int loc, const std::vector<glm::mat4>& values) const;
    void setUniformAt(int loc, float value) const;
    void setUniformAt(int loc, int value) const;

private:
    void destroy();
    void fetchAllActiveUniforms();
    void resolveLocations();
    void bindSamplerUnits();

    unsigned int m_programId = 0;
    std::unordered_map<std::string, int> m_uniformCache;
    Locations m_locations;
};
