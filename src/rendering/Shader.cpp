#include "Shader.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commands

void ShaderGlobals::Create()
{
    auto *device = Engine::Get()->GetGPUDevice();

    scenes.Create(device);
    materials.Create(device);
    objects.Create(device);
    skeletons.Create(device);
    lights.Create(device);
    shadow_maps.Create(device);
    env_probes.Create(device);
    env_grids.Create(device);
    immediate_draws.Create(device);
    entity_instance_batches.Create(device);
    cubemap_uniforms.Create(device, sizeof(CubemapUniforms));

    textures.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = Engine::Get()->GetGPUDevice();

    cubemap_uniforms.Destroy(device);

    env_probes.Destroy(device);
    env_grids.Destroy(device);

    scenes.Destroy(device);
    objects.Destroy(device);
    materials.Destroy(device);
    skeletons.Destroy(device);
    lights.Destroy(device);
    shadow_maps.Destroy(device);
    immediate_draws.Destroy(device);
    entity_instance_batches.Destroy(device);
}

struct RENDER_COMMAND(CreateShaderProgram) : RenderCommand
{
    ShaderProgramRef shader_program;
    CompiledShader compiled_shader;

    RENDER_COMMAND(CreateShaderProgram)(
        const ShaderProgramRef &shader_program,
        const CompiledShader &compiled_shader
    ) : shader_program(shader_program),
        compiled_shader(compiled_shader)
    {
    }

    virtual ~RENDER_COMMAND(CreateShaderProgram)() = default;

    virtual Result operator()()
    {
        if (!compiled_shader.IsValid()) {
            return { Result::RENDERER_ERR, "Invalid compiled shader" };
        }

        for (SizeType index = 0; index < compiled_shader.modules.Size(); index++) {
            const auto &byte_buffer = compiled_shader.modules[index];

            if (byte_buffer.Empty()) {
                continue;
            }

            HYPERION_BUBBLE_ERRORS(shader_program->AttachShader(
                Engine::Get()->GetGPUInstance()->GetDevice(),
                ShaderModule::Type(index),
                { byte_buffer }
            ));
        }

        return shader_program->Create(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(DestroyShaderProgram) : RenderCommand
{
    ShaderProgramRef shader_program;

    RENDER_COMMAND(DestroyShaderProgram)(const ShaderProgramRef &shader_program)
        : shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return shader_program->Destroy(Engine::Get()->GetGPUDevice());
    }
};

#pragma endregion

Shader::Shader()
    : EngineComponentBase(),
      m_shader_program(RenderObjects::Make<ShaderProgram>())
{
}

Shader::Shader(const CompiledShader &compiled_shader)
    : EngineComponentBase(),
      m_compiled_shader(compiled_shader),
      m_shader_program(RenderObjects::Make<ShaderProgram>())
{
}

Shader::~Shader()
{
    if (IsInitCalled()) {
        SetReady(false);
    }

    SafeRelease(std::move(m_shader_program));
}

void Shader::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    if (!m_compiled_shader.IsValid()) {
        return;
    }

    PUSH_RENDER_COMMAND(
        CreateShaderProgram,
        m_shader_program,
        m_compiled_shader
    );

    SetReady(true);
}

} // namespace hyperion
