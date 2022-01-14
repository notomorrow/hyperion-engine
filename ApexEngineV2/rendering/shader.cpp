#include "shader.h"
#include "../util/string_util.h"
#include "../util/shader_preprocessor.h"
#include "../util.h"

namespace apex {

Shader::Shader(const ShaderProperties &properties)
    : m_properties(properties),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    ResetUniforms();
}

Shader::Shader(const ShaderProperties &properties,
    const std::string &vscode,
    const std::string &fscode)
    : m_properties(properties),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    AddSubShader(SubShader(SubShaderType::SUBSHADER_VERTEX, vscode));
    AddSubShader(SubShader(SubShaderType::SUBSHADER_FRAGMENT, fscode));

    ResetUniforms();
}

Shader::~Shader()
{
    if (is_created) {
        glDeleteProgram(progid);

        for (auto &&sub : subshaders) {
            glDeleteShader(sub.second.id);
        }
    }
}

void Shader::ResetUniforms()
{
    SetUniform("HasDiffuseMap", 0);
    SetUniform("HasNormalMap", 0);
    SetUniform("HasParallaxMap", 0);
    SetUniform("HasAoMap", 0);
    SetUniform("HasBrdfMap", 0);
    SetUniform("HasMetalnessMap", 0);
    SetUniform("HasRoughnessMap", 0);
}

void Shader::ApplyMaterial(const Material &mat)
{
    ResetUniforms();

    MaterialFaceCull cull_mode(mat.cull_faces);

    if (m_override_cull != MaterialFaceCull::MaterialFace_None) {
        cull_mode = m_override_cull;
    }

    if (cull_mode == (MaterialFaceCull::MaterialFace_Front | MaterialFaceCull::MaterialFace_Back)) {
        glCullFace(GL_FRONT_AND_BACK);
    } else if (cull_mode & MaterialFaceCull::MaterialFace_Front) {
        glCullFace(GL_FRONT);
    } else if (cull_mode & MaterialFaceCull::MaterialFace_Back) {
        glCullFace(GL_BACK);
    } else if (cull_mode == MaterialFaceCull::MaterialFace_None) {
        glDisable(GL_CULL_FACE);
    }

    if (mat.alpha_blended) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (!mat.depth_test) {
        glDisable(GL_DEPTH_TEST);
    }
    if (!mat.depth_write) {
        glDepthMask(false);
    }
}

void Shader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    SetUniform("u_modelMatrix", transform.GetMatrix());
    SetUniform("u_viewMatrix", camera->GetViewMatrix());
    SetUniform("u_projMatrix", camera->GetProjectionMatrix());
}

void Shader::Use()
{
    if (!is_created) {
        progid = glCreateProgram();

        CatchGLErrors("Failed to create shader program.");

        for (auto &&sub : subshaders) {
            sub.second.id = glCreateShader(sub.first);

            CatchGLErrors("Failed to create subshader.");
        }

        is_created = true;
    }

    if (!is_uploaded) {
        for (auto &&it : subshaders) {
            auto &sub = it.second;

            const char *code_str = sub.code.c_str();
            glShaderSource(sub.id, 1, &code_str, NULL);
            glCompileShader(sub.id);
            glAttachShader(progid, sub.id);

            int status = -1;
            glGetShaderiv(sub.id, GL_COMPILE_STATUS, &status);

            if (!status) {
                int maxlen;
                glGetShaderiv(sub.id, GL_INFO_LOG_LENGTH, &maxlen);
                char *log = new char[maxlen];
                memset(log, 0, maxlen);
                glGetShaderInfoLog(sub.id, maxlen, NULL, log);

                std::cout << "In shader of class " << typeid(*this).name() << ":\n";
                std::cout << "\tShader compile error! ";
                std::cout << "\tCompile log: \n" << log << "\n";

                delete[] log;
            }
        }

        glBindFragDataLocation(progid, 0, "output0");
        glBindFragDataLocation(progid, 1, "output1");
        glBindFragDataLocation(progid, 2, "output2");
        CatchGLErrors("Failed to bind shader frag data.");

        glBindAttribLocation(progid, 0, "a_position");
        glBindAttribLocation(progid, 1, "a_normal");
        glBindAttribLocation(progid, 2, "a_texcoord0");
        glBindAttribLocation(progid, 3, "a_texcoord1");
        glBindAttribLocation(progid, 4, "a_tangent");
        glBindAttribLocation(progid, 5, "a_bitangent");
        glBindAttribLocation(progid, 6, "a_boneweights");
        glBindAttribLocation(progid, 7, "a_boneindices");
        CatchGLErrors("Failed to bind shader attributes.");

        glLinkProgram(progid);
        glValidateProgram(progid);

        int linked = 0;
        glGetProgramiv(progid, GL_LINK_STATUS, &linked);

        if (!linked) {
            int maxlen = 0;
            glGetProgramiv(progid, GL_INFO_LOG_LENGTH, &maxlen);

            if (maxlen != 0) {
                char *log = new char[maxlen];

                glGetProgramInfoLog(progid, maxlen, NULL, log);

                std::cout << "In shader of class " << typeid(*this).name() << ":\n";
                std::cout << "\tShader linker error! ";
                std::cout << "\tCompile log: \n" << log << "\n";

                glDeleteProgram(progid);

                delete[] log;

                return;
            }
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
                        uniform.second.data[2], uniform.second.data[3]);
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
    // m_override_cull = MaterialFaceCull::MaterialFace_None;
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(true);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glBlendFunc(GL_ONE, GL_ZERO);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
}

} // namespace apex
