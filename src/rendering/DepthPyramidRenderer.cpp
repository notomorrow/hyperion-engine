#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <Engine.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;

DepthPyramidRenderer::DepthPyramidRenderer()
    : m_is_rendered(false)
{
}

DepthPyramidRenderer::~DepthPyramidRenderer() = default;

void DepthPyramidRenderer::Create(AttachmentUsageRef depth_attachment_usage)
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    AssertThrow(m_depth_attachment_usage == nullptr);
    // AssertThrow(depth_attachment_usage->IsDepthAttachment());
    m_depth_attachment_usage = std::move(depth_attachment_usage);

    m_depth_pyramid_sampler = MakeRenderObject<renderer::Sampler>(
        FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    );

    HYPERION_ASSERT_RESULT(m_depth_pyramid_sampler->Create(g_engine->GetGPUDevice()));

    const renderer::Attachment *depth_attachment = m_depth_attachment_usage->GetAttachment();
    AssertThrow(depth_attachment != nullptr);

    const ImageRef &depth_image = depth_attachment->GetImage();
    AssertThrow(depth_image.IsValid());

    // create depth pyramid image
    m_depth_pyramid = MakeRenderObject<renderer::Image>(renderer::StorageImage(
        Extent3D {
            uint32(MathUtil::NextPowerOf2(depth_image->GetExtent().width)),
            uint32(MathUtil::NextPowerOf2(depth_image->GetExtent().height)),
            1
        },
        InternalFormat::R32F,
        ImageType::TEXTURE_TYPE_2D,
        FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,
        nullptr
    ));

    m_depth_pyramid->Create(g_engine->GetGPUDevice());

    m_depth_pyramid_view = MakeRenderObject<renderer::ImageView>();
    m_depth_pyramid_view->Create(g_engine->GetGPUDevice(), m_depth_pyramid);

    const uint num_mip_levels = m_depth_pyramid->NumMipmaps();
    m_depth_pyramid_mips.Reserve(num_mip_levels);

    for (uint mip_level = 0; mip_level < num_mip_levels; mip_level++) {
        ImageViewRef mip_image_view = MakeRenderObject<renderer::ImageView>();

        HYPERION_ASSERT_RESULT(mip_image_view->Create(
            g_engine->GetGPUDevice(),
            m_depth_pyramid,
            mip_level, 1,
            0, m_depth_pyramid->NumFaces()
        ));

        m_depth_pyramid_mips.PushBack(std::move(mip_image_view));
    }

    auto shader = g_shader_manager->GetOrCreate(HYP_NAME(GenerateDepthPyramid), { });
    const renderer::DescriptorTable descriptor_table = shader->GetCompiledShader().GetDefinition().GetDescriptorUsages().BuildDescriptorTable();

    const renderer::DescriptorSetDeclaration *depth_pyramid_descriptor_set_decl = descriptor_table.FindDescriptorSetDeclaration(HYP_NAME(DepthPyramidDescriptorSet));
    AssertThrow(depth_pyramid_descriptor_set_decl != nullptr);

    renderer::DescriptorSetLayout depth_pyramid_layout(*depth_pyramid_descriptor_set_decl);
    
    for (uint i = 0; i < max_frames_in_flight; i++) {
        for (uint mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            // create descriptor sets for depth pyramid generation.
            DescriptorSet2Ref depth_pyramid_descriptor_set = depth_pyramid_layout.CreateDescriptorSet();

            if (mip_level == 0) {
                // first mip level -- input is the actual depth image
                depth_pyramid_descriptor_set->SetElement("InImage", m_depth_attachment_usage->GetImageView());
            } else {
                depth_pyramid_descriptor_set->SetElement("InImage", m_depth_pyramid_mips[mip_level - 1]);
            }

            depth_pyramid_descriptor_set->SetElement("OutImage", m_depth_pyramid_mips[mip_level]);
            depth_pyramid_descriptor_set->SetElement("DepthPyramidSampler", m_depth_pyramid_sampler);

            HYPERION_ASSERT_RESULT(depth_pyramid_descriptor_set->Create(g_engine->GetGPUDevice()));

            m_depth_pyramid_descriptor_sets[i].PushBack(std::move(depth_pyramid_descriptor_set));
        }
    }
    // create compute pipeline for rendering depth image
    m_generate_depth_pyramid = MakeRenderObject<renderer::ComputePipeline>(
        shader->GetShaderProgram(),
        Array<DescriptorSet2Ref> { m_depth_pyramid_descriptor_sets[0].Front() }
    );

    DeferCreate(m_generate_depth_pyramid, g_engine->GetGPUDevice());
}

void DepthPyramidRenderer::Destroy()
{
    SafeRelease(std::move(m_depth_pyramid));
    SafeRelease(std::move(m_depth_pyramid_view));
    SafeRelease(std::move(m_depth_pyramid_mips));

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        SafeRelease(std::move(m_depth_pyramid_descriptor_sets[frame_index]));
    }

    SafeRelease(std::move(m_depth_pyramid_sampler));

    SafeRelease(std::move(m_depth_attachment_usage));

    m_is_rendered = false;
}

void DepthPyramidRenderer::Render(Frame *frame)
{
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    DebugMarker marker(command_buffer, "Depth pyramid generation");

    const auto num_depth_pyramid_mip_levels = m_depth_pyramid_mips.Size();

    const Extent3D &image_extent = m_depth_attachment_usage->GetAttachment()->GetImage()->GetExtent();
    const Extent3D &depth_pyramid_extent = m_depth_pyramid->GetExtent();

    uint32 mip_width = image_extent.width,
        mip_height = image_extent.height;

    for (uint mip_level = 0; mip_level < num_depth_pyramid_mip_levels; mip_level++) {
        // level 0 == write just-rendered depth image into mip 0

        // put the mip into writeable state
        m_depth_pyramid->GetGPUImage()->InsertSubResourceBarrier(
            command_buffer,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::ResourceState::UNORDERED_ACCESS
        );
        
        const uint32 prev_mip_width = mip_width,
            prev_mip_height = mip_height;

        mip_width = MathUtil::Max(1u, depth_pyramid_extent.width >> (mip_level));
        mip_height = MathUtil::Max(1u, depth_pyramid_extent.height >> (mip_level));

        // bind descriptor set to compute pipeline
        m_depth_pyramid_descriptor_sets[frame_index][mip_level]->Bind(command_buffer, m_generate_depth_pyramid, 0);

        // set push constant data for the current mip level
        m_generate_depth_pyramid->Bind(
            command_buffer,
            Pipeline::PushConstantData {
                .depth_pyramid_data = {
                    .mip_dimensions = renderer::ShaderVec2<uint32>(mip_width, mip_height),
                    .prev_mip_dimensions = renderer::ShaderVec2<uint32>(prev_mip_width, prev_mip_height),
                    .mip_level = mip_level
                }
            }
        );
        
        // dispatch to generate this mip level
        m_generate_depth_pyramid->Dispatch(
            command_buffer,
            Extent3D {
                (mip_width + 31) / 32,
                (mip_height + 31) / 32,
                1
            }
        );

        // put this mip into readable state
        m_depth_pyramid->GetGPUImage()->InsertSubResourceBarrier(
            command_buffer,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::ResourceState::SHADER_RESOURCE
        );
    }

    // all mip levels have been transitioned into this state
    m_depth_pyramid->GetGPUImage()->SetResourceState(
        renderer::ResourceState::SHADER_RESOURCE
    );

    m_is_rendered = true;
}

} // namespace hyperion::v2