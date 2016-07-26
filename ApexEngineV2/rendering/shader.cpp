#include "shader.h"
#include "../util/string_util.h"
#include "../util/shader_preprocessor.h"
#include <iostream>

namespace apex {
Shader::Shader(const std::string &vscode, const std::string &fscode)
{
    is_uploaded = false;
    is_created = false;
    uniform_changed = false;
    subshaders.push_back(SubShader(CoreEngine::VERTEX_SHADER, vscode));
    subshaders.push_back(SubShader(CoreEngine::FRAGMENT_SHADER, fscode));
}

Shader::~Shader()
{
    auto *engine = CoreEngine::GetInstance();
    if (engine != nullptr && is_created) {
        engine->DeleteProgram(progid);
        for (auto &&sub : subshaders) {
            engine->DeleteShader(sub.id);
        }
    }
}

void Shader::ApplyMaterial(const Material &mat)
{
}

void Shader::ApplyTransforms(const Matrix4 &model, const Matrix4 &view, const Matrix4 &proj)
{
    SetUniform("u_modelMatrix", model);
    SetUniform("u_viewMatrix", view);
    SetUniform("u_projMatrix", proj);
}

void Shader::SetUniform(const std::string &name, float value)
{
    uniforms[name] = Uniform(value);
    uniform_changed = true;
}

void Shader::SetUniform(const std::string &name, int value)
{
    uniforms[name] = Uniform(value);
    uniform_changed = true;
}

void Shader::SetUniform(const std::string &name, const Vector2 &value)
{
    uniforms[name] = Uniform(value);
    uniform_changed = true;
}

void Shader::SetUniform(const std::string &name, const Vector3 &value)
{
    uniforms[name] = Uniform(value);
    uniform_changed = true;
}

void Shader::SetUniform(const std::string &name, const Vector4 &value)
{
    uniforms[name] = Uniform(value);
    uniform_changed = true;
}

void Shader::SetUniform(const std::string &name, const Matrix4 &value)
{
    uniforms[name] = Uniform(value);
    uniform_changed = true;
}

void Shader::Use()
{
    auto *engine = CoreEngine::GetInstance();

    if (!is_created) {
        progid = engine->CreateProgram();
        for (auto &&sub : subshaders) {
            sub.id = engine->CreateShader(sub.type);
        }
        is_created = true;
    }
    if (!is_uploaded) {
        for (auto &&sub : subshaders) {
            const char *code_str = sub.code.c_str();
            engine->ShaderSource(sub.id, 1, &code_str, NULL);
            engine->CompileShader(sub.id);
            engine->AttachShader(progid, sub.id);

            int status = -1;
            engine->GetShaderiv(sub.id, CoreEngine::COMPILE_STATUS, &status);

            if (status == false) {
                int maxlen;
                engine->GetShaderiv(sub.id, CoreEngine::INFO_LOG_LENGTH, &maxlen);

                char *log = new char[maxlen];
                engine->GetShaderInfoLog(sub.id, maxlen, &maxlen, log);

                std::cout << "Shader compile error! ";
                std::cout << "Compile log: \n" << log << "\n";

                delete[] log;
            }
        }

        engine->BindAttribLocation(progid, 0, "a_position");
        engine->BindAttribLocation(progid, 1, "a_normal");
        engine->BindAttribLocation(progid, 2, "a_texcoord0");
        engine->BindAttribLocation(progid, 3, "a_texcoord1");
        engine->BindAttribLocation(progid, 4, "a_tangent");
        engine->BindAttribLocation(progid, 5, "a_bitangent");
        engine->BindAttribLocation(progid, 6, "a_boneweights");
        engine->BindAttribLocation(progid, 7, "a_boneindices");

        engine->LinkProgram(progid);
        engine->ValidateProgram(progid);

        int linked = 0;
        engine->GetProgramiv(progid, CoreEngine::LINK_STATUS, &linked);
        if (linked == false) {
            int maxlen = 0;
            engine->GetProgramiv(progid, CoreEngine::INFO_LOG_LENGTH, &maxlen);

            char *log = new char[maxlen];

            engine->GetProgramInfoLog(progid, maxlen, &maxlen, log);
            engine->DeleteProgram(progid);
            std::cout << "Log: \n " << log << "\n\n";

            delete[] log;
        }

        is_uploaded = true;
    }

    engine->UseProgram(progid);

    if (uniform_changed) {
        uniform_changed = false;
        for (auto &&uniform : uniforms) {
            int loc = engine->GetUniformLocation(progid, uniform.first.c_str());
            if (loc != -1) {
                switch (uniform.second.type) {
                case Uniform::UF_FLOAT:
                    engine->Uniform1f(loc, uniform.second.data[0]);
                    break;
                case Uniform::UF_INT:
                    engine->Uniform1i(loc, (int)uniform.second.data[0]);
                    break;
                case Uniform::UF_VEC2:
                    engine->Uniform2f(loc, uniform.second.data[0], uniform.second.data[1]);
                    break;
                case Uniform::UF_VEC3:
                    engine->Uniform3f(loc, uniform.second.data[0], uniform.second.data[1],
                        uniform.second.data[2]);
                    break;
                case Uniform::UF_VEC4:
                    engine->Uniform4f(loc, uniform.second.data[0], uniform.second.data[1],
                        uniform.second.data[2], uniform.second.data[2]);
                    break;
                case Uniform::UF_MAT4:
                {
                    engine->UniformMatrix4fv(loc, 1, true, &uniform.second.data[0]);
                    break;
                }
                default:
                    std::cout << "invalid uniform: " << uniform.first << "\n";
                    break;
                }
            }
        }
    }
}

void Shader::End()
{
    CoreEngine::GetInstance()->UseProgram(NULL);
}
}