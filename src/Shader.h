#pragma once
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    unsigned int id = 0;
    Shader() = default;
    Shader(const char* vertPath, const char* fragPath);
    void use() const;
    void setVec4(const char* name, float r, float g, float b, float a) const;
    void setVec3(const char* name, float x, float y, float z) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setMat4(const char* name, const float* mat) const;
    void setMat4(const char* name, const glm::mat4& mat) const;
    void setFloat(const char* name, float v) const;
    void setInt(const char* name, int v) const;
private:
    unsigned int compile(unsigned int type, const std::string& src);
};
