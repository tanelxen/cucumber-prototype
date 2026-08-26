//
//  ShaderVariant.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.08.26.
//

#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

class ShaderVariant
{
public:
    ShaderVariant();

    ShaderVariant(const ShaderVariant&) = delete;             // no copy allowed
    ShaderVariant& operator =(const ShaderVariant&) = delete; // no copy allowed

    ShaderVariant(ShaderVariant&& other) noexcept;            // move allowed
    ShaderVariant& operator=(ShaderVariant&& other) noexcept; // move allowed

    ~ShaderVariant();

public:
    void init(const char* filename); // hack!!!
    void init(const char* vert_src, const char* frag_src);

    void bind() const;
    void unbind() const;
    
    void setUniform(const std::string& name, bool value) const;
    void setUniform(const std::string& name, float value) const;
    void setUniform(const std::string &name, int value) const;

    void setUniform(const std::string& name, const glm::vec3& vector) const;
    void setUniform(const std::string& name, const glm::vec4& vector) const;
    void setUniform(const std::string& name, const glm::mat4& matrix) const;
    
    void setUniform(const std::string& name, const std::vector<glm::vec3>& vectors) const;
    void setUniform(const std::string& name, const std::vector<glm::mat4>& matrices) const;
    
    int getProgramId() const { return m_programId; }

private:
    int m_programId;
    
    int getUniformLocation(const std::string& name) const;
    void fetchAllActiveUniforms();
    
    mutable std::unordered_map<std::string, int> m_uniformCache;
};
