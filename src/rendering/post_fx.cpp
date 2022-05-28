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



PostProcessingEffect::PostProcessingEffect()
    : EngineComponentBase()
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
    for (auto &effect : m_effects) {
        AssertThrow(effect.second != nullptr);

        effect.second->Init(engine);
    }
}

void PostProcessing::Destroy(Engine *engine)
{
    m_effects.Clear();
}

void PostProcessing::Render(Engine *engine, Frame *frame) const
{
    Engine::AssertOnThread(THREAD_RENDER);

    for (auto &it : m_effects) {
        auto &effect = it.second;

        effect->GetFullScreenPass().Record(engine, frame->GetFrameIndex());
        effect->GetFullScreenPass().Render(engine, frame);
    }
}
} // namespace hyperion