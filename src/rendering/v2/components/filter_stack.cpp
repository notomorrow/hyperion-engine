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

/*void FilterStack::AddFilter(int priority, std::unique_ptr<Filter> &&filter)
{
    m_filters.emplace_back(priority, std::move(filter));

    std::sort(m_filters.begin(), m_filters.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
}*/

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
        Shader::ID shader_id = engine->AddShader(std::make_unique<Shader>(std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_vert.spv").Read()}},
            SubShader{ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_frag.spv").Read()}}
        }));

        m_filters[i] = std::make_unique<Filter>(shader_id);
        m_filters[i]->CreateRenderPass(engine);
        m_filters[i]->CreateFrameData(engine);
        m_filters[i]->CreateDescriptors(engine, binding_index);
    }
}

void FilterStack::Destroy(Engine *engine)
{
    /* Render passes + framebuffer cleanup are handled by the engine */
    for (auto &filter : m_filters) {
        filter->Destroy(engine);
    }
}

void FilterStack::BuildPipelines(Engine *engine)
{
    for (auto &m_filter : m_filters) {
        m_filter->CreatePipeline(engine);
    }
}

void FilterStack::Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index)
{
    for (const auto &filter : m_filters) {
        filter->Record(engine, frame_index);
        filter->Render(engine, primary_command_buffer, frame_index);
    }
}
} // namespace hyperion