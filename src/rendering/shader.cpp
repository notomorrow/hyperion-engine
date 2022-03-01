#include "shader.h"
#include "environment.h"
#include "../util/string_util.h"
#include "../util/shader_preprocessor.h"
#include "../util.h"
#include "../gl_util.h"

namespace hyperion {

std::string EnumerateLines(const std::string &str);

Shader::Shader(const ShaderProperties &properties)
    : m_properties(properties),
      m_previous_properties_hash_code(properties.GetHashCode().Value()),
      m_override_cull(MaterialFaceCull::MaterialFace_None),
      is_uploaded(false),
      is_created(false),
      uniform_changed(false)
{
    InitUniforms();
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

    InitUniforms();
}

Shader::~Shader()
{
    DestroyGpuData();
}

void Shader::InitUniforms()
{
    m_uniform_flip_uv_x = m_uniforms.Acquire("FlipUV_X").id;
    m_uniform_flip_uv_y = m_uniforms.Acquire("FlipUV_Y").id;
    m_uniform_uv_scale = m_uniforms.Acquire("UVScale").id;

    m_uniform_viewport = m_uniforms.Acquire("Viewport").id;

    // matrices
    m_uniform_model_matrix = m_uniforms.Acquire("u_modelMatrix").id;
    m_uniform_view_matrix = m_uniforms.Acquire("u_viewMatrix").id;
    m_uniform_proj_matrix = m_uniforms.Acquire("u_projMatrix").id;
    m_uniform_view_proj_matrix = m_uniforms.Acquire("u_viewProjMatrix").id;

    // lights

    m_uniform_num_point_lights = m_uniforms.Acquire("env_NumPointLights").id;

    m_uniform_directional_light.uniform_direction = m_uniforms.Acquire("env_DirectionalLight.direction", Uniform(Vector3())).id;
    m_uniform_directional_light.uniform_color = m_uniforms.Acquire("env_DirectionalLight.color", Uniform(Vector4())).id;
    m_uniform_directional_light.uniform_intensity = m_uniforms.Acquire("env_DirectionalLight.intensity", Uniform(100000.0f)).id;

    // textures
    EnumOptions<MaterialTextureKey, const char *, MATERIAL_MAX_TEXTURES> has_texture_names({
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_DIFFUSE_MAP, "HasDiffuseMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_NORMAL_MAP, "HasNormalMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_AO_MAP, "HasAoMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_PARALLAX_MAP, "HasParallaxMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_METALNESS_MAP, "HasMetalnessMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, "HasRoughnessMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_SKYBOX_MAP, "HasSkyboxMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_COLOR_MAP, "HasColorMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_POSITION_MAP, "HasPositionMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_DATA_MAP, "HasDataMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_SSAO_MAP, "HasSSLightingMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TANGENT_MAP, "HasTangentMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_BITANGENT_MAP, "HasBitangentMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_DEPTH_MAP, "HasDepthMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_SPLAT_MAP, "HasSplatMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP, "HasBaseTerrainColorMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP, "HasBaseTerrainNormalMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP, "HasBaseTerrainAoMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP, "HasBaseTerrainParallaxMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP, "HasTerrain1ColorMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP, "HasTerrain1NormalMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP, "HasTerrain1AoMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP, "HasTerrain1ParallaxMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP, "HasTerrain2ColorMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP, "HasTerrain2NormalMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP, "HasTerrain2AoMap"),
        std::make_pair(MaterialTextureKey::MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP, "HasTerrain2ParallaxMap")
    });

    for (int i = 0; i < MATERIAL_MAX_TEXTURES; i++) {
        if (!Material::material_texture_names.HasAt(i)) {
            m_uniform_textures[i] = -1;
            m_uniform_has_textures[i] = -1;
            continue;
        }

        auto pair = Material::material_texture_names.KeyValueAt(i);
        m_uniform_textures[i] = m_uniforms.Acquire(pair.second, Uniform((const Texture*)nullptr)).id;
        m_uniform_has_textures[i] = m_uniforms.Acquire(has_texture_names.Get(pair.first)).id;
    }

    ResetUniforms();
}

void Shader::CreateGpuData()
{
    AssertThrow(!is_created);

    progid = glCreateProgram();

    CatchGLErrors("Failed to create shader program.");

    for (auto &&sub : subshaders) {
        sub.second.id = glCreateShader(sub.first);

        CatchGLErrors("Failed to create subshader.");
    }

    is_created = true;
}

void Shader::UploadGpuData()
{
    AssertThrow(is_created && !is_uploaded);

    for (auto &&it : subshaders) {
        auto &sub = it.second;

        const char *code_str = sub.processed_code.c_str();
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

            std::cout << "Pre-processed shader code:\n\n";

            for (auto&& it : subshaders) {
                std::cout << "===================\n";

                switch (it.first) {
                case SubShaderType::SUBSHADER_FRAGMENT:
                    std::cout << "  FRAGMENT SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_VERTEX:
                    std::cout << "   VERTEX SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_GEOMETRY:
                    std::cout << "  GEOMETRY SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_COMPUTE:
                    std::cout << "  COMPUTE SHADER\n";
                    break;
                }

                std::cout << "===================\n";
                std::cout << EnumerateLines(it.second.processed_code);

                std::cout << "\n\n";
            }

            std::cout << "\n\n\n";

            std::cout << "In shader of class " << typeid(*this).name() << ":\n";
            std::cout << "\tShader compile error! ";
            std::cout << "\tCompile log: \n" << log << "\n";


            delete[] log;
        }
    }

    glBindFragDataLocation(progid, 0, "output0");
    glBindFragDataLocation(progid, 1, "output1");
    glBindFragDataLocation(progid, 2, "output2");
    glBindFragDataLocation(progid, 3, "output3");
    glBindFragDataLocation(progid, 4, "output4");
    glBindFragDataLocation(progid, 5, "output5");
    glBindFragDataLocation(progid, 6, "output6");
    glBindFragDataLocation(progid, 7, "output7");
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

            std::cout << "Pre-processed shader code:\n\n";

            for (auto &&it : subshaders) {
                std::cout << "===================\n";

                switch (it.first) {
                case SubShaderType::SUBSHADER_FRAGMENT:
                    std::cout << "  FRAGMENT SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_VERTEX:
                    std::cout << "   VERTEX SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_GEOMETRY:
                    std::cout << "  GEOMETRY SHADER\n";
                    break;
                case SubShaderType::SUBSHADER_COMPUTE:
                    std::cout << "  COMPUTE SHADER\n";
                }

                std::cout << "===================\n";
                std::cout << EnumerateLines(it.second.processed_code);

                std::cout << "\n\n";
            }

            glGetProgramInfoLog(progid, maxlen, NULL, log);

            std::cout << "\n\n\n";

            std::cout << "In shader of class " << typeid(*this).name() << ":\n";
            std::cout << "\tShader linker error! ";
            std::cout << "\tCompile log: \n" << log << "\n";

            glDeleteProgram(progid);

            delete[] log;

            return;
        }
    }

    is_uploaded = true;

    CatchGLErrors("Failed to use upload shader GPU data");
}

void Shader::DestroyGpuData()
{
    DestroyUniformBufferObjects();

    if (is_created) {
        glDeleteProgram(progid);

        for (auto &&sub : subshaders) {
            glDeleteShader(sub.second.id);
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
    for (int i = 0; i < MATERIAL_MAX_TEXTURES; i++) {
        if (m_uniform_has_textures[i] == -1) {
            continue;
        }

        SetUniform(m_uniform_has_textures[i], 0);
    }

    SetUniform(m_uniform_uv_scale, Vector2(1));
}


void Shader::SetLightUniforms(Environment *env)
{
    size_t num_point_lights = env->NumVisiblePointLights();

    SetUniform(m_uniform_num_point_lights, int(num_point_lights));

    if (num_point_lights > m_uniform_point_lights.size()) {
        size_t prev_size = m_uniform_point_lights.size();
        m_uniform_point_lights.resize(num_point_lights);

        for (size_t i = prev_size; i < num_point_lights; i++) {
            std::string index_string(std::to_string(i));

            m_uniform_point_lights[i].uniform_color = m_uniforms.Acquire("env_PointLights[" + index_string + "].color").id;
            m_uniform_point_lights[i].uniform_position = m_uniforms.Acquire("env_PointLights[" + index_string + "].position").id;
            m_uniform_point_lights[i].uniform_radius = m_uniforms.Acquire("env_PointLights[" + index_string + "].radius").id;
            m_uniform_point_lights[i].uniform_intensity = m_uniforms.Acquire("env_PointLights[" + index_string + "].intensity").id;
        }
    }

    for (size_t i = 0; i < num_point_lights; i++) {
        const auto &pl = env->GetVisiblePointLights()[i];

        SetUniform(m_uniform_point_lights[i].uniform_color, pl.point_light->GetColor());
        SetUniform(m_uniform_point_lights[i].uniform_position, pl.point_light->GetPosition());
        SetUniform(m_uniform_point_lights[i].uniform_radius, pl.point_light->GetRadius());
    }

    SetUniform(m_uniform_directional_light.uniform_direction, env->GetSun().GetDirection());
    SetUniform(m_uniform_directional_light.uniform_color, env->GetSun().GetColor());
    SetUniform(m_uniform_directional_light.uniform_intensity, env->GetSun().GetIntensity());
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
    } else if (cull_mode == MaterialFaceCull::MaterialFace_Front) {
        glCullFace(GL_FRONT);
    } else if (cull_mode == MaterialFaceCull::MaterialFace_Back) {
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

    const auto &flip_uv = mat.GetParameter(MATERIAL_PARAMETER_FLIP_UV);
    SetUniform(m_uniform_flip_uv_x, int(flip_uv[0]));
    SetUniform(m_uniform_flip_uv_y, int(flip_uv[1]));

    SetUniform(m_uniform_uv_scale, Vector2(mat.GetParameter(MATERIAL_PARAMETER_UV_SCALE)[0], mat.GetParameter(MATERIAL_PARAMETER_UV_SCALE)[1]));

    for (size_t i = 0; i < MATERIAL_MAX_TEXTURES; i++) {
        bool has_texture = mat.GetTextures().HasAt(i);

        if (has_texture) {
            auto texture = mat.GetTextures().ValueAt(i).m_texture.get();

            if (texture == nullptr) {
                continue;
            }

            texture->Prepare();

            if (m_uniform_textures[i] != -1) {
                SetUniform(m_uniform_textures[i], texture);
            }
        }

        if (m_uniform_has_textures[i] != -1) {
            SetUniform(m_uniform_has_textures[i], int(has_texture));
        }
    }
}

void Shader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    SetUniform(m_uniform_model_matrix, transform.GetMatrix());
    SetUniform(m_uniform_view_matrix, camera->GetViewMatrix());
    SetUniform(m_uniform_proj_matrix, camera->GetProjectionMatrix());
    SetUniform(m_uniform_view_proj_matrix, camera->GetViewProjectionMatrix());
    SetUniform(m_uniform_viewport, Vector2(camera->GetWidth(), camera->GetHeight()));
    SetUniform(m_uniform_num_point_lights, 0);
}

void Shader::ApplyUniforms()
{
    if (uniform_changed) {
        int texture_index = 1;

        for (auto &it : m_uniforms.m_uniforms) {
            const auto &uniform = it.first;

            /*if (!it.second && !uniform.value.IsTextureType()) {
                // has not changed so we skip it
                continue;
            }*/

            it.second = false; // set to changed = false;

            it.first.value.BindUniform(this, it.first.name.c_str(), texture_index);
        }

        for (auto &it : m_uniforms.m_uniform_buffers) {
            auto &uniform_buffer = it.first;

            if (uniform_buffer._internal == nullptr) {
                uniform_buffer._internal = non_owning_ptr(const_cast<const Shader::UniformBuffer::Internal*>(CreateUniformBufferInternal(uniform_buffer)));
            }

            AssertContinueMsg(uniform_buffer._internal->generated, "Uniform buffer not generated, may be in an error state.");

            glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer._internal->handle);
            CatchGLErrors("Failed to bind uniform buffer");

            const size_t buffer_size = uniform_buffer._internal->size;
            size_t total_size = 0, offset = 0;

            for (size_t i = 0; i < uniform_buffer.data.size(); i++) {
                AssertBreakMsg(total_size < buffer_size, "Total buffer size reached capacity!");

                size_t size = uniform_buffer.data[i].value.GetSize();
                glBufferSubData(GL_UNIFORM_BUFFER, offset, size, uniform_buffer.data[i].value.GetRawPtr());
                CatchGLErrors("Failed to set uniform buffer subdata");

                total_size += size;
                offset += MathUtil::NextMultiple(size, 16);
            }

            glBindBufferBase(GL_UNIFORM_BUFFER, uniform_buffer._internal->index, uniform_buffer._internal->handle);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            it.second = false; // set to changed = false;

            // TODO: shared data across programs
        }

        uniform_changed = false;
    }
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
            PreprocessSubShader(sub_shader.second, m_properties);
        }

        CreateGpuData();
        UploadGpuData();
        CreateUniformBufferObjects(); // rebuild UBOs

        m_previous_properties_hash_code = m_properties.GetHashCode().Value();
    }

    glUseProgram(progid);
    CatchGLErrors("Failed to use shader program");

    ApplyUniforms();
}

void Shader::End()
{
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

Shader::UniformBuffer::Internal *Shader::CreateUniformBufferInternal(UniformBuffer &uniform_buffer)
{
    size_t total_size = 0; // calculate total size of data allocated for uniform buffer
    for (size_t i = 0; i < uniform_buffer.data.size(); i++) {
        total_size += uniform_buffer.data[i].value.GetSize();
    }

    UniformBuffer::Internal *_internal = new UniformBuffer::Internal;
    _internal->generated = false;

    // TODO: assert total_size fits into block_size?
    GLuint block_index = glGetUniformBlockIndex(progid, uniform_buffer.name.c_str());
    CatchGLErrors("Failed to get uniform block index");

    if (block_index != GL_INVALID_INDEX) {
        GLint block_size;
        glGetActiveUniformBlockiv(progid, block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &block_size);
        CatchGLErrors("Failed to get active uniform block size");

        _internal->size = block_size;

        GLuint handle;
        glGenBuffers(1, &handle);
        CatchGLErrors("Failed to generate uniform buffer");

        glBindBuffer(GL_UNIFORM_BUFFER, handle);
        glBufferData(GL_UNIFORM_BUFFER, total_size, nullptr, GL_STATIC_DRAW);
        CatchGLErrors("Failed to set uniform buffer initial data");
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        _internal->handle = handle;
        _internal->index = block_index;
        _internal->generated = true;
    }
    

    m_uniform_buffer_internals.push_back(_internal);

    return _internal;
}

void Shader::DestroyUniformBufferInternal(Shader::UniformBuffer::Internal *_internal)
{
    auto it = std::find(
        m_uniform_buffer_internals.begin(),
        m_uniform_buffer_internals.end(),
        _internal
    );

    AssertThrow(it != m_uniform_buffer_internals.end());

    if (_internal->generated) {
        GLuint handle = _internal->handle;
        glDeleteBuffers(1, &handle);
        CatchGLErrors("Failed to delete uniform buffer");
    }

    delete _internal;

    m_uniform_buffer_internals.erase(it);
}

void Shader::CreateUniformBufferObjects()
{
    for (auto &it : m_uniforms.m_uniform_buffers) {
        if (it.first._internal != nullptr) {
            continue;
        }

        it.first._internal = non_owning_ptr(const_cast<const Shader::UniformBuffer::Internal*>(CreateUniformBufferInternal(it.first)));
    }
}

void Shader::DestroyUniformBufferObjects()
{
    for (auto *uniform_buffer : m_uniform_buffer_internals) {
        DestroyUniformBufferInternal(uniform_buffer);
    }

    m_uniform_buffer_internals.clear();

    for (auto &it : m_uniforms.m_uniform_buffers) {
        // destroyed above
        it.first._internal = nullptr;
    }
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
    PreprocessSubShader(sub_shader, properties);
    subshaders[type] = sub_shader;
}

void Shader::PreprocessSubShader(SubShader &sub_shader, const ShaderProperties &properties)
{
    ShaderProperties defines(properties);

    sub_shader.processed_code = ShaderPreprocessor::ProcessShader(sub_shader.code, defines, sub_shader.path);
}

std::string EnumerateLines(const std::string &str)
{
    std::string result;
    result.reserve(str.length());

    std::vector<std::string> lines = StringUtil::Split(str, '\n');

    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].length() == 0) {
            if (i != 0 && lines[i - 1].length() == 0) {
                // no-op
            } else if (i != lines.size() - 1 && lines[i + 1].length() == 0) {
                result += " ... ";
            } 
        } else {
            result += "[" + std::to_string(i + 1) + "]: " + lines[i];
        }

        result += "\n";
    }

    return result;
}

} // namespace hyperion
