#include "filter_stack.h"
#include "../engine.h"
#include "filter.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>
#include <util/mesh_factory.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::DescriptorSet;

FilterStack::FilterStack()
    : m_filters{}
{
}

FilterStack::~FilterStack() = default;

void FilterStack::Create(Engine *engine)
{
    /* Add a renderpass per each filter */
    static const int num_filters = 2;
    static const std::array<std::string, 2> filter_shader_names = {  "filter_pass", "deferred" };
    m_filters.resize(num_filters);

    uint32_t binding_index = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL)->GetDescriptors().size();

    /* TODO: use subpasses for gbuffer so we only have num_filters * num_frames descriptors */
    for (int i = 0; i < num_filters; i++) {
        Shader::ID shader_id = engine->AddShader(std::make_unique<Shader>(std::vector<SpirvObject>{
            SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_vert.spv").Read() },
            SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_frag.spv").Read() }
        }));

        m_filters[i] = std::make_unique<Filter>(shader_id);
        m_filters[i]->CreateRenderPass(engine);
        m_filters[i]->CreateFrameData(engine);
        m_filters[i]->CreateDescriptors(engine, binding_index);
    }

    /* Finalize descriptor pool */
    auto descriptor_pool_result = engine->GetInstance()->GetDescriptorPool().Create(engine->GetInstance()->GetDevice());
    AssertThrowMsg(descriptor_pool_result, "%s", descriptor_pool_result.message);

    for (int i = 0; i < num_filters; i++) {
        m_filters[i]->CreatePipeline(engine);
    }
}

void FilterStack::Destroy(Engine *engine)
{
    /* Render passes + framebuffer cleanup are handled by the engine */
    for (const auto &filter : m_filters) {
        filter->Destroy(engine);
    }
}

void FilterStack::Render(Engine *engine, Frame *frame, uint32_t frame_index)
{
    for (auto &filter : m_filters) {
        //if (!filter->IsRecorded()) {
            filter->Record(engine, frame_index);
        //}

        filter->Render(engine, frame, frame_index);
    }
}
} // namespace hyperion