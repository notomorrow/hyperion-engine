#include "post_fx.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>
#include <builders/mesh_builder.h>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::DescriptorKey;
using renderer::SamplerDescriptor;


PostProcessingEffect::PostProcessingEffect(Stage stage, uint index)
    : EngineComponentBase(),
      m_full_screen_pass(nullptr, stage == Stage::PRE_SHADING ? DescriptorKey::POST_FX_PRE_STACK : DescriptorKey::POST_FX_POST_STACK, index),
      m_stage(stage),
      m_is_enabled(true)
{
}

PostProcessingEffect::~PostProcessingEffect()
{
    Teardown();
}

void PostProcessingEffect::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    m_shader = CreateShader(engine);
    m_shader.Init();

    m_full_screen_pass.SetShader(m_shader.IncRef());
    m_full_screen_pass.Create(engine);

    SetReady(true);

    OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
        SetReady(false);

        m_full_screen_pass.Destroy(engine);
        m_shader = nullptr;
    }), engine);
}

PostProcessing::PostProcessing()  = default;
PostProcessing::~PostProcessing() = default;

void PostProcessing::Create(Engine *engine)
{
    for (auto &effect : m_pre_effects) {
        AssertThrow(effect.second != nullptr);

        effect.second->Init(engine);
    }
    
    for (auto &effect : m_post_effects) {
        AssertThrow(effect.second != nullptr);

        effect.second->Init(engine);
    }

    CreateUniformBuffer(engine);
}

void PostProcessing::Destroy(Engine *engine)
{
    auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL);
    descriptor_set_globals->GetDescriptor(DescriptorKey::POST_FX_UNIFORMS)
        ->RemoveSubDescriptor(0);

    HYPERION_ASSERT_RESULT(m_uniform_buffer.Destroy(engine->GetDevice()));

    m_pre_effects.Clear();
    m_post_effects.Clear();
}

void PostProcessing::CreateUniformBuffer(Engine *engine)
{
    Engine::AssertOnThread(THREAD_RENDER);
    
    HYPERION_ASSERT_RESULT(m_uniform_buffer.Create(engine->GetDevice(), sizeof(PostProcessingUniforms)));

    PostProcessingUniforms post_processing_uniforms{
        .num_pre_effects  = static_cast<uint>(m_pre_effects.Size()),
        .num_post_effects = static_cast<uint>(m_post_effects.Size())
    };
    
    for (auto &it : m_pre_effects) {
        if (it.second != nullptr && it.second->IsEnabled()) {
            AssertThrowMsg(it.second->GetIndex() != ~0u, "Not yet initialized - index not set yet");

            post_processing_uniforms.pre_effect_enabled_mask |= 1u << it.second->GetIndex();
            post_processing_uniforms.last_enabled_pre_effect_index = MathUtil::Max(
                post_processing_uniforms.last_enabled_pre_effect_index,
                it.second->GetIndex()
            );
        }
    }

    for (auto &it : m_post_effects) {
        if (it.second != nullptr && it.second->IsEnabled()) {
            AssertThrowMsg(it.second->GetIndex() != ~0u, "Not yet initialized - index not set yet");

            post_processing_uniforms.post_effect_enabled_mask |= 1u << it.second->GetIndex();
            post_processing_uniforms.last_enabled_post_effect_index = MathUtil::Max(
                post_processing_uniforms.last_enabled_post_effect_index,
                it.second->GetIndex()
            );
        }
    }

    m_uniform_buffer.Copy(
        engine->GetDevice(),
        sizeof(PostProcessingUniforms),
        &post_processing_uniforms
    );

    auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_GLOBAL);
    descriptor_set_globals->AddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::POST_FX_UNIFORMS)
        ->AddSubDescriptor({
            .element_index = 0,
            .buffer        = &m_uniform_buffer
        });
}

void PostProcessing::RenderPre(Engine *engine, Frame *frame) const
{
    Engine::AssertOnThread(THREAD_RENDER);

    for (auto &it : m_pre_effects) {
        auto &effect = it.second;

        effect->GetFullScreenPass().Record(engine, frame->GetFrameIndex());
        effect->GetFullScreenPass().Render(engine, frame);
    }
}

void PostProcessing::RenderPost(Engine *engine, Frame *frame) const
{
    Engine::AssertOnThread(THREAD_RENDER);

    for (auto &it : m_post_effects) {
        auto &effect = it.second;

        effect->GetFullScreenPass().Record(engine, frame->GetFrameIndex());
        effect->GetFullScreenPass().Render(engine, frame);
    }
}
} // namespace hyperion