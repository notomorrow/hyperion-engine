#include "shader.h"
#include "../util/string_util.h"
#include "../util/shader_preprocessor.h"
#include <iostream>

namespace apex {

Shader::Shader(const ShaderProperties &properties)
    : is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
}

Shader::Shader(const ShaderProperties &properties, 
    const std::string &vscode, const std::string &fscode) 
    : is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    subshaders.push_back(SubShader(GL_VERTEX_SHADER, vscode));
    subshaders.push_back(SubShader(GL_FRAGMENT_SHADER, fscode));
}

Shader::~Shader()
{
    if (is_created) {
        glDeleteProgram(progid);
        for (auto &&sub : subshaders) {
            glDeleteShader(sub.id);
        }
    }
}

void Shader::ApplyMaterial(const Material &mat)
{
}

void Shader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    SetUniform("u_modelMatrix", transform);
    SetUniform("u_viewMatrix", camera->GetViewMatrix());
    SetUniform("u_projMatrix", camera->GetProjectionMatrix());
}

void Shader::Use()
{
    if (!is_created) {
        progid = glCreateProgram();
        for (auto &&sub : subshaders) {
            sub.id = glCreateShader(sub.type);
        }
        is_created = true;
    }
    if (!is_uploaded) {
        for (auto &&sub : subshaders) {
            const char *code_str = sub.code.c_str();
            glShaderSource(sub.id, 1, &code_str, NULL);
            glCompileShader(sub.id);
            glAttachShader(progid, sub.id);

            int status = -1;
            glGetShaderiv(sub.id, GL_COMPILE_STATUS, &status);

            if (status == false) {
                int maxlen;
                glGetShaderiv(sub.id, GL_INFO_LOG_LENGTH, &maxlen);

                char *log = new char[maxlen];
                glGetShaderInfoLog(sub.id, maxlen, &maxlen, log);

                std::cout << "Shader compile error! ";
                std::cout << "Compile log: \n" << log << "\n";

                delete[] log;
            }
        }

        glBindAttribLocation(progid, 0, "a_position");
        glBindAttribLocation(progid, 1, "a_normal");
        glBindAttribLocation(progid, 2, "a_texcoord0");
        glBindAttribLocation(progid, 3, "a_texcoord1");
        glBindAttribLocation(progid, 4, "a_tangent");
        glBindAttribLocation(progid, 5, "a_bitangent");
        glBindAttribLocation(progid, 6, "a_boneweights");
        glBindAttribLocation(progid, 7, "a_boneindices");

        glLinkProgram(progid);
        glValidateProgram(progid);

        int linked = 0;
        glGetProgramiv(progid, GL_LINK_STATUS, &linked);
        if (linked == false) {
            int maxlen = 0;
            glGetProgramiv(progid, GL_INFO_LOG_LENGTH, &maxlen);

            char *log = new char[maxlen];

            glGetProgramInfoLog(progid, maxlen, &maxlen, log);
            glDeleteProgram(progid);
            std::cout << "Log: \n " << log << "\n\n";

            delete[] log;
        }

        is_uploaded = true;
    }

    glUseProgram(progid);

    if (uniform_changed) {
        for (auto &&uniform : uniforms) {
            int loc = glGetUniformLocation(progid, uniform.first.c_str());
            if (loc != -1) {
                switch (uniform.second.type) {
                case Uniform::Uniform_Float:
                    glUniform1f(loc, uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Int:
                    glUniform1i(loc, (int)uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Vector2:
                    glUniform2f(loc, uniform.second.data[0], uniform.second.data[1]);
                    break;
                case Uniform::Uniform_Vector3:
                    glUniform3f(loc, uniform.second.data[0], uniform.second.data[1],
                        uniform.second.data[2]);
                    break;
                case Uniform::Uniform_Vector4:
                    glUniform4f(loc, uniform.second.data[0], uniform.second.data[1],
                        uniform.second.data[2], uniform.second.data[2]);
                    break;
                case Uniform::Uniform_Matrix4:
                    glUniformMatrix4fv(loc, 1, true, &uniform.second.data[0]);
                    break;
                default:
                    std::cout << "invalid uniform: " << uniform.first << "\n";
                    break;
                }
            }
        }
        uniform_changed = false;
    }
}

void Shader::End()
{
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(true);

    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
}

} // namespace apex