#include "shader.h"
#include "../util/string_util.h"
#include "../util/shader_preprocessor.h"
#include "../core_engine.h"
#include "../gl_util.h"

namespace apex {

Shader::Shader(const ShaderProperties &properties)
    : m_properties(properties),
      m_previous_properties_hash_code(properties.GetHashCode().Value()),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    // ResetUniforms();
}

Shader::Shader(const ShaderProperties &properties,
    const std::string &vscode,
    const std::string &fscode)
    : m_properties(properties),
      m_previous_properties_hash_code(properties.GetHashCode().Value()),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    AddSubShader(SubShaderType::SUBSHADER_VERTEX, vscode, properties, "");
    AddSubShader(SubShaderType::SUBSHADER_FRAGMENT, fscode, properties, "");

    // ResetUniforms();
}

Shader::~Shader()
{
    DestroyGpuData();
}

void Shader::CreateGpuData()
{
    assert(!is_created);

    progid = CoreEngine::GetInstance()->CreateProgram();
    std::cout << "Create program " << progid << " " << typeid(*this).name() << "\n";
    CatchGLErrors("Failed to create shader program.");

    for (auto &&sub : subshaders) {
        sub.second.id = CoreEngine::GetInstance()->CreateShader(sub.first);

        CatchGLErrors("Failed to create subshader.");
    }

    is_created = true;
}

void Shader::UploadGpuData()
{
    assert(is_created && !is_uploaded);

    for (auto &&it : subshaders) {
        auto &sub = it.second;

        const char *code_str = sub.processed_code.c_str();
        CoreEngine::GetInstance()->ShaderSource(sub.id, 1, &code_str, NULL);
        CoreEngine::GetInstance()->CompileShader(sub.id);
        CoreEngine::GetInstance()->AttachShader(progid, sub.id);

        int status = -1;
        CoreEngine::GetInstance()->GetShaderiv(sub.id, GL_COMPILE_STATUS, &status);

        if (!status) {
            int maxlen;
            CoreEngine::GetInstance()->GetShaderiv(sub.id, GL_INFO_LOG_LENGTH, &maxlen);
            char *log = new char[maxlen];
            memset(log, 0, maxlen);
            CoreEngine::GetInstance()->GetShaderInfoLog(sub.id, maxlen, NULL, log);

            std::cout << "In shader of class " << typeid(*this).name() << ":\n";
            std::cout << "\tShader compile error! ";
            std::cout << "\tCompile log: \n" << log << "\n";

            delete[] log;
        }
    }

    // CoreEngine::GetInstance()->BindFragDataLocation(progid, 0, "output0");
    // CoreEngine::GetInstance()->BindFragDataLocation(progid, 1, "output1");
    // CoreEngine::GetInstance()->BindFragDataLocation(progid, 2, "output2");

    // CatchGLErrors("Failed to bind shader frag data.");

    // CoreEngine::GetInstance()->BindAttribLocation(progid, 0, "a_position");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 1, "a_normal");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 2, "a_texcoord0");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 3, "a_texcoord1");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 4, "a_tangent");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 5, "a_bitangent");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 6, "a_boneweights");
    // CoreEngine::GetInstance()->BindAttribLocation(progid, 7, "a_boneindices");

    // CatchGLErrors("Failed to bind shader attributes.");

    CoreEngine::GetInstance()->LinkProgram(progid);
   /*CoreEngine::GetInstance()->ValidateProgram(progid);

    int linked = 0;
    CoreEngine::GetInstance()->GetProgramiv(progid, GL_LINK_STATUS, &linked);

    if (!linked) {
        int maxlen = 0;
        CoreEngine::GetInstance()->GetProgramiv(progid, GL_INFO_LOG_LENGTH, &maxlen);

        if (maxlen != 0) {
            char *log = new char[maxlen];

            CoreEngine::GetInstance()->GetProgramInfoLog(progid, maxlen, NULL, log);

            std::cout << "In shader of class " << typeid(*this).name() << ":\n";
            std::cout << "\tShader linker error! ";
            std::cout << "\tCompile log: \n" << log << "\n";

            CoreEngine::GetInstance()->DeleteProgram(progid);

            delete[] log;

            return;
        }
    }*/

    is_uploaded = true;
}

void Shader::DestroyGpuData()
{
    if (is_created) {
        CoreEngine::GetInstance()->DeleteProgram(progid);

        for (auto &&sub : subshaders) {
            CoreEngine::GetInstance()->DeleteShader(sub.second.id);
        }
    }

    is_created = false;
    is_uploaded = false;
}

bool Shader::ShaderPropertiesChanged() const
{
    // TODO: memoization
    return m_properties.GetHashCode().Value() != m_previous_properties_hash_code;
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
    // ResetUniforms();

    MaterialFaceCull cull_mode(mat.cull_faces);

    if (m_override_cull != MaterialFaceCull::MaterialFace_None) {
        cull_mode = m_override_cull;
    }

    if (cull_mode == (MaterialFaceCull::MaterialFace_Front | MaterialFaceCull::MaterialFace_Back)) {
        CoreEngine::GetInstance()->CullFace(GL_FRONT_AND_BACK);
    } else if (cull_mode & MaterialFaceCull::MaterialFace_Front) {
        CoreEngine::GetInstance()->CullFace(GL_FRONT);
    } else if (cull_mode & MaterialFaceCull::MaterialFace_Back) {
        CoreEngine::GetInstance()->CullFace(GL_BACK);
    } else if (cull_mode == MaterialFaceCull::MaterialFace_None) {
        CoreEngine::GetInstance()->Disable(GL_CULL_FACE);
    }

    if (mat.alpha_blended) {
        // CoreEngine::GetInstance()->Enable(GL_BLEND);
        CoreEngine::GetInstance()->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (!mat.depth_test) {
        CoreEngine::GetInstance()->Disable(GL_DEPTH_TEST);
    }

    if (!mat.depth_write) {
        CoreEngine::GetInstance()->DepthMask(false);
    }
}

void Shader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    SetUniform("u_modelMatrix", transform.GetMatrix());
    SetUniform("u_viewMatrix", camera->GetViewMatrix());
    SetUniform("u_projMatrix", camera->GetProjectionMatrix());
    SetUniform("u_viewProjMatrix", camera->GetViewProjectionMatrix());
}

void Shader::Use()
{
    if (!is_created) {
        CreateGpuData();
    }

    if (!is_uploaded) {
        UploadGpuData();
    } else if (ShaderPropertiesChanged()) {
        DestroyGpuData();

        for (auto &sub_shader : subshaders) {
            ReprocessSubShader(sub_shader.second, m_properties);
        }

        CreateGpuData();
        UploadGpuData();

        m_previous_properties_hash_code = m_properties.GetHashCode().Value();
    }

    CoreEngine::GetInstance()->UseProgram(progid);
    std::cout << "Use program " << progid << "\n";

    if (uniform_changed) {
        int texture_index = 1;

        for (auto &&uniform : uniforms) { // TODO:  delete uniform buffers when one removed (if this works)
            std::cout << "in uniform " << uniform.first << "\n";
            unsigned int buf_id;
            if (uniform_buf_ids.find(uniform.first) == uniform_buf_ids.end()) {
                std::cout << "not set, creating buffer\n";
                CoreEngine::GetInstance()->GenBuffers(1, &buf_id);
                std::cout <<" genbuffers done, id = " << buf_id << "\n";
                CoreEngine::GetInstance()->BindBuffer(CoreEngine::GLEnums::UNIFORM_BUFFER, buf_id);
                std::cout <<" generated buffer, setting data\n";
                switch (uniform.second.type) {
                case Uniform::Uniform_Float:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(float), uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Int:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(int), uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Vector2:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(float) * 2, uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Vector3:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(float) * 3, uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Vector4:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(float) * 4, uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Matrix4:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(float) * 16, uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Texture2D:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(int), uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    break;
                case Uniform::Uniform_Texture3D:
                    CoreEngine::GetInstance()->BufferData(CoreEngine::GLEnums::UNIFORM_BUFFER, sizeof(int), uniform.second.data.data(), CoreEngine::GLEnums::STATIC_DRAW);
                    // CoreEngine::GetInstance()->Uniform1i(loc, texture_index);
                    break;
                default:
                    std::cout << "invalid uniform: " << uniform.first << "\n";
                    break;
                }
                CoreEngine::GetInstance()->BindBuffer(CoreEngine::GLEnums::UNIFORM_BUFFER, 0);
                uniform_buf_ids[uniform.first] = buf_id;
            } else {
                buf_id = uniform_buf_ids[uniform.first];
            }

            std::cout << "lookup " << uniform.first << ":\n";
            int loc = CoreEngine::GetInstance()->GetUniformBlockIndex(progid, uniform.first.c_str());//CoreEngine::GetInstance()->GetUniformLocation(progid, uniform.first.c_str());
        
            std::cout << "get location for " << uniform.first << ": " << loc << "\n";
            if (loc != -1) {
                CoreEngine::GetInstance()->BindBuffer(CoreEngine::GLEnums::UNIFORM_BUFFER, buf_id);
                switch (uniform.second.type) {
                case Uniform::Uniform_Float:
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(float), uniform.second.data.data());
                    // CoreEngine::GetInstance()->Uniform1f(loc, uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Int:
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(int), uniform.second.data.data());
                    // CoreEngine::GetInstance()->Uniform1i(loc, (int)uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Vector2:
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(float) * 2, uniform.second.data.data());
                    // CoreEngine::GetInstance()->Uniform2f(loc, uniform.second.data[0], uniform.second.data[1]);
                    break;
                case Uniform::Uniform_Vector3:
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(float) * 3, uniform.second.data.data());
                    // CoreEngine::GetInstance()->Uniform3f(loc, uniform.second.data[0], uniform.second.data[1],
                        // uniform.second.data[2]);
                    break;
                case Uniform::Uniform_Vector4:
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(float) * 4, uniform.second.data.data());
                    // CoreEngine::GetInstance()->Uniform4f(loc, uniform.second.data[0], uniform.second.data[1],
                    //     uniform.second.data[2], uniform.second.data[3]);
                    break;
                case Uniform::Uniform_Matrix4:
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(float) * 16, uniform.second.data.data());
                    // CoreEngine::GetInstance()->UniformMatrix4fv(loc, 1, true, &uniform.second.data[0]);
                    break;
                case Uniform::Uniform_Texture2D:
                    Texture::ActiveTexture(texture_index);
                    CoreEngine::GetInstance()->BindTexture(GL_TEXTURE_2D, int(uniform.second.data[0]));
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(int), &texture_index);
                    // CoreEngine::GetInstance()->Uniform1i(loc, texture_index);
                    texture_index++;
                    break;
                case Uniform::Uniform_Texture3D:
                    Texture::ActiveTexture(texture_index);
                    CoreEngine::GetInstance()->BindTexture(GL_TEXTURE_CUBE_MAP, int(uniform.second.data[0]));
                    CoreEngine::GetInstance()->BufferSubData(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, sizeof(int), &texture_index);
                    // CoreEngine::GetInstance()->Uniform1i(loc, texture_index);
                    texture_index++;
                    break;
                default:
                    std::cout << "invalid uniform: " << uniform.first << "\n";
                    break;
                }
                // CoreEngine::GetInstance()->BindBufferBase(CoreEngine::GLEnums::UNIFORM_BUFFER, 0, 0);
                CatchGLErrors((uniform.first + ": Failed to set uniform").c_str(), false);
            }
            CoreEngine::GetInstance()->BindBuffer(CoreEngine::GLEnums::UNIFORM_BUFFER, 0);
            CoreEngine::GetInstance()->BindBufferBase(CoreEngine::GLEnums::UNIFORM_BUFFER, loc, buf_id);
        }

        std::cout <<"end update uniforms\n";

        uniform_changed = false;
    }
}

void Shader::End()
{
    // CoreEngine::GetInstance()->Disable(GL_BLEND);
    CoreEngine::GetInstance()->Enable(GL_DEPTH_TEST);
    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(GL_CULL_FACE);
    CoreEngine::GetInstance()->CullFace(GL_BACK);
    CoreEngine::GetInstance()->BlendFunc(GL_ONE, GL_ZERO);
    CoreEngine::GetInstance()->BindTexture(GL_TEXTURE_2D, 0);

    // CoreEngine::GetInstance()->UseProgram(0);
}

void Shader::AddSubShader(SubShaderType type,
    const std::string &code,
    const ShaderProperties &properties,
    const std::string &path)
{
    SubShader sub_shader;
    sub_shader.id = 0;
    sub_shader.type = type;
    sub_shader.code = code;
    sub_shader.path = path;
    sub_shader.processed_code = ShaderPreprocessor::ProcessShader(code, properties, path);
    subshaders[type] = sub_shader;
}

void Shader::ReprocessSubShader(SubShader &sub_shader, const ShaderProperties &properties)
{
    sub_shader.processed_code = ShaderPreprocessor::ProcessShader(sub_shader.code, properties, sub_shader.path);
}

} // namespace apex
