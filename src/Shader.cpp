#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

static std::string readFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[Shader] Cannot open: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

unsigned int Shader::compile(unsigned int type, const std::string& src) {
    unsigned int s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "[Shader] Compile error:\n" << log << "\n";
    }
    return s;
}

Shader::Shader(const char* vertPath, const char* fragPath) {
    std::string vsrc = readFile(vertPath);
    std::string fsrc = readFile(fragPath);
    unsigned int vs = compile(GL_VERTEX_SHADER, vsrc);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fsrc);
    id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    int ok;
    glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(id, 512, nullptr, log);
        std::cerr << "[Shader] Link error:\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void Shader::use() const { glUseProgram(id); }

void Shader::setVec4(const char* name, float r, float g, float b, float a) const {
    glUniform4f(glGetUniformLocation(id, name), r, g, b, a);
}

void Shader::setVec3(const char* name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(id, name), x, y, z);
}

void Shader::setVec3(const char* name, const glm::vec3& value) const {
    setVec3(name, value.x, value.y, value.z);
}

void Shader::setMat4(const char* name, const float* mat) const {
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, mat);
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const {
    setMat4(name, &mat[0][0]);
}

void Shader::setFloat(const char* name, float v) const {
    glUniform1f(glGetUniformLocation(id, name), v);
}
void Shader::setInt(const char* name, int v) const {
    glUniform1i(glGetUniformLocation(id, name), v);
}
