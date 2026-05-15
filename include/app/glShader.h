#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>

class glShader {
public:
    GLuint ID = 0;

    glShader() = default;

    glShader(std::string_view vertexShaderPath, std::string_view fragmentShaderPath) {
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;

        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try {
            vShaderFile.open(vertexShaderPath);
            fShaderFile.open(fragmentShaderPath);

            std::stringstream vShaderStream, fShaderStream;
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();

            vShaderFile.close();
            fShaderFile.close();

            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure &e) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << e.what() << std::endl;
        }

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        GLuint vertexShader, fragmentShader;

        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vShaderCode, NULL);
        glCompileShader(vertexShader);
        checkCompileErrors(vertexShader, "VERTEX");

        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fShaderCode, NULL);
        glCompileShader(fragmentShader);
        checkCompileErrors(fragmentShader, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertexShader);
        glAttachShader(ID, fragmentShader);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }


    ~glShader() {
        if (ID != 0) glDeleteProgram(ID);
    }

    glShader(const glShader&) = delete;
    glShader& operator=(const glShader&) = delete;

    glShader(glShader&& other) noexcept : ID(other.ID) {
        other.ID = 0;
    }

    glShader& operator=(glShader&& other) noexcept {
        if (this != &other) {
            if (ID != 0) glDeleteProgram(ID);
            ID = other.ID;
            other.ID = 0;
        }
        return *this;
    }

    void use() const { glUseProgram(ID); }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

private:
    static void checkCompileErrors(GLuint shader, std::string type) {
        GLint success;
        GLchar info[1024];

        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, info);
                std::cout << "ERROR::SHADER::" << type << "::COMPILATION_\n" << info << std::endl;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);

            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, info);
                std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << info << std::endl;
            }
        }
    }
};
