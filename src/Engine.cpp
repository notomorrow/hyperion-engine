#include "Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/Compute.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/vct/VoxelConeTracing.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>
#include <asset/material_loaders/MTLMaterialLoader.hpp>
#include <asset/model_loaders/OgreXMLModelLoader.hpp>
#include <asset/skeleton_loaders/OgreXMLSkeletonLoader.hpp>
#include <asset/texture_loaders/TextureLoader.hpp>
#include <asset/audio_loaders/WAVAudioLoader.hpp>
#include <asset/script_loaders/ScriptLoader.hpp>

#include <Game.hpp>

#include <builders/MeshBuilder.hpp>

#include <audio/AudioManager.hpp>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;
using renderer::DescriptorKey;
using renderer::FillMode;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : shader_globals(nullptr),
      m_instance(new Instance(_system, app_name, "HyperionEngine")),
      m_asset_manager(this),
      m_shader_compiler(this)
{
    RegisterDefaultAssetLoaders();
}

Engine::~Engine()
{
    m_placeholder_data.Destroy(this);

    HYP_FLUSH_RENDER_QUEUE(this); // just to clear anything remaining up 

    AssertThrow(m_instance != nullptr);
    (void)m_instance->GetDevice()->Wait();

    if (shader_globals != nullptr) {
        shader_globals->Destroy(this);

        delete shader_globals;
    }

    m_instance->Destroy();
}

void Engine::RegisterDefaultAssetLoaders()
{
    m_asset_manager.SetBasePath(FilePath::Join(HYP_ROOT_DIR, "res"));

    m_asset_manager.Register<OBJModelLoader>("obj");
    m_asset_manager.Register<OgreXMLModelLoader>("mesh.xml");
    m_asset_manager.Register<OgreXMLSkeletonLoader>("skeleton.xml");
    m_asset_manager.Register<TextureLoader>(
        "png", "jpg", "jpeg", "tga",
        "bmp", "psd", "gif", "hdr", "tif"
    );
    m_asset_manager.Register<MTLMaterialLoader>("mtl");
    m_asset_manager.Register<WAVAudioLoader>("wav");
    m_asset_manager.Register<ScriptLoader>("hypscript");
    m_asset_manager.Register<FBOMModelLoader>("fbom");
}

void Engine::FindTextureFormatDefaults()
{
    Threads::AssertOnThread(THREAD_RENDER);

    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ //InternalFormat::TEXTURE_INTERNAL_FORMAT_R10G10B10A2,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER_8BIT,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_NORMALS,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_R11G11B10F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_UV,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F,
                        InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_UNUSED,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_R8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
        )
    );
}

void Engine::PrepareFinalPass()
{
    m_full_screen_quad = CreateHandle<Mesh>(MeshBuilder::Quad());
    AssertThrow(InitObject(m_full_screen_quad));

    auto shader = CreateHandle<Shader>(m_shader_compiler.GetCompiledShader("FinalOutput", ShaderProps { }));

    AssertThrow(InitObject(shader));

    UInt iteration = 0;
    
    auto render_pass = CreateHandle<RenderPass>(
        renderer::RenderPassStage::PRESENT,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    );

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_instance->swapchain->image_format,
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(m_instance->GetDevice()));
    }

    for (VkImage img : m_instance->swapchain->images) {
        auto fbo = std::make_unique<Framebuffer>(
            m_instance->swapchain->extent,
            Handle<RenderPass>(render_pass)
        );

        renderer::AttachmentRef *color_attachment_ref,
            *depth_attachment_ref;

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentRef(
            m_instance->GetDevice(),
            img,
            renderer::helpers::ToVkFormat(m_instance->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_ref
        ));

        color_attachment_ref->SetBinding(0);

        fbo->GetFramebuffer().AddAttachmentRef(color_attachment_ref);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentRef(
            m_instance->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_ref
        ));

        fbo->GetFramebuffer().AddAttachmentRef(depth_attachment_ref);

        depth_attachment_ref->SetBinding(1);

        if (iteration == 0) {
            render_pass->GetRenderPass().AddAttachmentRef(color_attachment_ref);
            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment_ref);

            InitObject(render_pass);

            m_root_pipeline = CreateHandle<RendererInstance>(
                std::move(shader),
                Handle<RenderPass>(render_pass),
                RenderableAttributeSet(
                    MeshAttributes {
                        .vertex_attributes = renderer::static_mesh_vertex_attributes
                    },
                    MaterialAttributes {
                        .bucket = BUCKET_SWAPCHAIN
                    }
                )
            );
        }

        m_root_pipeline->AddFramebuffer(CreateHandle<Framebuffer>(fbo.release()));

        ++iteration;
    }

    callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        m_render_list_container.AddFramebuffersToPipelines(this);
        InitObject(m_root_pipeline);
    });
}

void Engine::Initialize()
{
    Threads::AssertOnThread(THREAD_MAIN);

    m_crash_handler.Initialize();

    task_system.Start();

#ifdef HYP_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    //SetThreadAffinityMask(GetCurrentThread(), (1 << 8));
#endif

    HYPERION_ASSERT_RESULT(m_instance->Initialize(use_debug_layers));

    FindTextureFormatDefaults();

    m_configuration.SetToDefaultConfiguration(this);

    if (!m_shader_compiler.LoadShaderDefinitions()) {
        HYP_BREAKPOINT;
    }

    shader_globals = new ShaderGlobals(m_instance->GetFrameHandler()->NumFrames());
    shader_globals->Create(this);

    m_placeholder_data.Create(this);

    m_world.Init(this);
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->scenes.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(SceneShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->lights.GetBuffers()[0].get(),
            .range  = static_cast<UInt>(sizeof(LightDrawProxy))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::SHADOW_MATRICES)
        ->SetSubDescriptor({ .buffer = shader_globals->shadow_maps.GetBuffers()[0].get() });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->materials.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(MaterialShaderData))
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(ObjectShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->SetSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[0].get(),
            .range = static_cast<UInt>(sizeof(SkeletonShaderData))
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->scenes.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(SceneShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->lights.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(LightDrawProxy))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::SHADOW_MATRICES)
        ->SetSubDescriptor({
            .buffer = shader_globals->shadow_maps.GetBuffers()[1].get()
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .buffer = shader_globals->materials.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(MaterialShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(ObjectShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->SetSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[1].get(),
            .range = static_cast<UInt>(sizeof(SkeletonShaderData))
        });

#if HYP_FEATURES_BINDLESS_TEXTURES
    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);
#else
    auto *material_sampler_descriptor = m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
        ->AddDescriptor<renderer::SamplerDescriptor>(renderer::DescriptorKey::SAMPLER);

    material_sampler_descriptor->SetSubDescriptor({
        .sampler = &GetPlaceholderData().GetSamplerLinear()
    });

    auto *material_textures_descriptor = m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES)
        ->AddDescriptor<renderer::ImageDescriptor>(renderer::DescriptorKey::TEXTURES);

    for (UInt i = 0; i < DescriptorSet::max_material_texture_samplers; i++) {
        material_textures_descriptor->SetSubDescriptor({
            .element_index = i,
            .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
        });
    }
#endif

    for (UInt frame_index = 0; frame_index < static_cast<UInt>(std::size(DescriptorSet::global_buffer_mapping)); frame_index++) {
        const auto descriptor_set_index = DescriptorSet::global_buffer_mapping[frame_index];

        auto *descriptor_set = GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(descriptor_set_index);

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::CUBEMAP_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer = &shader_globals->cubemap_uniforms
            });

        auto *env_probe_textures_descriptor = descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::ENV_PROBE_TEXTURES);

        for (UInt env_probe_index = 0; env_probe_index < max_bound_env_probes; env_probe_index++) {
            env_probe_textures_descriptor->SetSubDescriptor({
                .element_index = env_probe_index,
                .image_view = &GetPlaceholderData().GetImageViewCube1x1R8()
            });
        }

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::ENV_PROBES)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer = GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(GetDevice(), sizeof(EnvProbeShaderData))
            });

        // ssao/gi combined result image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // ui placeholder image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::UI_TEXTURE)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // placeholder rt radiance image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // placeholder rt probe system uniforms
        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::RT_PROBE_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = 0,
                .buffer = GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(GetDevice(), sizeof(ProbeSystemUniforms))
            });

        // placeholder rt probes irradiance image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::RT_IRRADIANCE_GRID)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // placeholder rt probes irradiance image
        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::RT_DEPTH_GRID)
            ->SetSubDescriptor({
                .element_index = 0,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });
    }

    // add placeholder shadowmaps
    for (DescriptorSet::Index descriptor_set_index : DescriptorSet::scene_buffer_mapping) {
        auto *descriptor_set = GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(descriptor_set_index);

        auto *shadow_map_descriptor = descriptor_set
            ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);
        
        for (UInt i = 0; i < /*max_shadow_maps*/ 1; i++) {
            shadow_map_descriptor->SetSubDescriptor({
                .element_index = i,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8(),
                .sampler = &GetPlaceholderData().GetSamplerNearest()
            });
        }
    }

    // add VCT descriptor placeholders
    auto *vct_descriptor_set = GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);
    
#if 1
    // voxel image
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(0)
        ->SetSubDescriptor({
            .element_index = 0u,
            .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8Storage()
        });

    // voxel uniforms
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(1)
        ->SetSubDescriptor({
            .element_index = 0u,
            .buffer = GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(GetDevice(), sizeof(VoxelUniforms))
        });

    // temporal blend image
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(6)
        ->SetSubDescriptor({
            .element_index = 0u,
            .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8Storage()
        });
    // voxel image (texture3D)
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::ImageDescriptor>(7)
        ->SetSubDescriptor({
            .element_index = 0u,
            .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8()
        });
    // voxel sampler
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::SamplerDescriptor>(8)
        ->SetSubDescriptor({
            .element_index = 0u,
            .sampler = &GetPlaceholderData().GetSamplerLinear()
        });

#else // svo tests
    // atomic counter
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(0)
        ->SetSubDescriptor({
            .element_index = 0u,
            .buffer = GetPlaceholderData().GetOrCreateBuffer<renderer::AtomicCounterBuffer>(GetDevice(), sizeof(UInt32))
        });

    // fragment list
    vct_descriptor_set
        ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(1)
        ->SetSubDescriptor({
            .element_index = 0u,
            .buffer = GetPlaceholderData().GetOrCreateBuffer<renderer::StorageBuffer>(GetDevice(), sizeof(ShaderVec2<UInt32>))
        });
#endif
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_globals = GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageSamplerDescriptor>(DescriptorKey::VOXEL_IMAGE)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView3D1x1x1R8Storage(),
                .sampler = &GetPlaceholderData().GetSamplerLinear()
            });

        // add placeholder SSR image
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_FINAL_TEXTURE)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        // sparse voxel octree buffer
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::SVO_BUFFER)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = GetPlaceholderData().GetOrCreateBuffer<renderer::StorageBuffer>(GetDevice(), sizeof(ShaderVec2<UInt32>))
            });

        { // add placeholder gbuffer textures
            auto *gbuffer_textures = descriptor_set_globals->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::GBUFFER_TEXTURES);

            UInt element_index = 0u;

            // not including depth texture here
            for (UInt attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                gbuffer_textures->SetSubDescriptor({
                    .element_index = element_index,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

                ++element_index;
            }

            // add translucent bucket's albedo
            gbuffer_textures->SetSubDescriptor({
                .element_index = element_index,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

            ++element_index;
        }

        { // more placeholder gbuffer stuff, different slots
            // depth attachment goes into separate slot
            /* Depth texture */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            /* Mip chain */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            /* Gbuffer depth sampler */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_DEPTH_SAMPLER)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .sampler = &GetPlaceholderData().GetSamplerNearest()
                });

            /* Gbuffer sampler */
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .sampler = &GetPlaceholderData().GetSamplerLinear()
                });

            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });
                
            descriptor_set_globals
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        { // SSR placeholders
            /* SSR Data */
            descriptor_set_globals // 1st stage -- trace, write UVs
                ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_UV_IMAGE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            descriptor_set_globals // 2nd stage -- sample
                ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_SAMPLE_IMAGE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            descriptor_set_globals // 2nd stage -- write radii
                ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_RADIUS_IMAGE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            descriptor_set_globals // 3rd stage -- blur horizontal
                ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_BLUR_HOR_IMAGE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            descriptor_set_globals // 3rd stage -- blur vertical
                ->GetOrAddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_BLUR_VERT_IMAGE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            /* SSR Data */
            descriptor_set_globals // 1st stage -- trace, write UVs
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_UV_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
               });

            descriptor_set_globals // 2nd stage -- sample
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_SAMPLE_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
               });

            descriptor_set_globals // 2nd stage -- write radii
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_RADIUS_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
               });

            descriptor_set_globals // 3rd stage -- blur horizontal
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_BLUR_HOR_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });

            descriptor_set_globals // 3rd stage -- blur vertical
                ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_BLUR_VERT_TEXTURE)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        { // POST FX processing placeholders
            
            for (const auto descriptor_key : { DescriptorKey::POST_FX_PRE_STACK, DescriptorKey::POST_FX_POST_STACK }) {
                auto *descriptor = descriptor_set_globals->GetOrAddDescriptor<renderer::ImageDescriptor>(descriptor_key);

                for (UInt effect_index = 0; effect_index < 8; effect_index++) {
                    descriptor->SetSubDescriptor({
                        .element_index = effect_index,
                        .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
                    });
                }
            }
        }
    }


#if 0//HYP_FEATURES_ENABLE_RAYTRACING
    { // add RT placeholders
        auto *rt_descriptor_set = GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING);

        rt_descriptor_set->GetOrAddDescriptor<renderer::StorageImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        rt_descriptor_set->GetOrAddDescriptor<renderer::StorageImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });

        rt_descriptor_set->GetOrAddDescriptor<renderer::StorageImageDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &GetPlaceholderData().GetImageView2D1x1R8()
            });
    }
#endif

    /* for textures */
    //shader_globals->textures.Create(this);
    
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().Create(m_instance->GetDevice()));

    m_render_list_container.Create(this);

    AssertThrowMsg(AudioManager::GetInstance()->Initialize(), "Failed to initialize audio device");

    m_running.store(true);

    PrepareFinalPass();
}

void Engine::Compile()
{
    Threads::AssertOnThread(THREAD_MAIN);

    for (UInt i = 0; i < m_instance->GetFrameHandler()->NumFrames(); i++) {
        /* Finalize env probes */
        shader_globals->env_probes.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize shadow maps */
        shader_globals->shadow_maps.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize lights */
        shader_globals->lights.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize skeletons */
        shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize materials */
        shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize scene data */
        shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), i);
    }

    callbacks.TriggerPersisted(EngineCallback::CREATE_DESCRIPTOR_SETS, this);
    
    m_deferred_renderer.Create(this);
    
    /* Finalize descriptor pool */
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().CreateDescriptorSets(m_instance->GetDevice()));
    DebugLog(
        LogType::Debug,
        "Finalized descriptor pool\n"
    );

    HYP_FLUSH_RENDER_QUEUE(this);

    callbacks.TriggerPersisted(EngineCallback::CREATE_GRAPHICS_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_COMPUTE_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_RAYTRACING_PIPELINES, this);

    m_is_render_loop_active = true;
}

void Engine::RequestStop()
{
    m_running.store(false);
}

void Engine::FinalizeStop()
{
    Threads::AssertOnThread(THREAD_MAIN);

    SafeReleaseHandle<Mesh>(std::move(m_full_screen_quad));

    m_is_stopping = true;
    m_is_render_loop_active = false;
    task_system.Stop();

    AssertThrow(GetInstance()->GetDevice()->Wait());

    while (game_thread.IsRunning()) {
        HYP_FLUSH_RENDER_QUEUE(this);
    }

    game_thread.Join();

    m_render_list_container.Destroy(this);
    m_deferred_renderer.Destroy(this);

    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(m_instance->GetDevice()));
    }

    m_safe_deleter.ForceReleaseAll(this);

    HYP_FLUSH_RENDER_QUEUE(this);

    m_renderer_instance_mapping.Clear();

    HYP_FLUSH_RENDER_QUEUE(this);

    AssertThrow(GetInstance()->GetDevice()->Wait());
}

void Engine::RenderNextFrame(Game *game)
{
    if (!m_running.load()) {
        FinalizeStop();

        return;
    }

    auto frame_result = GetInstance()->GetFrameHandler()->PrepareFrame(
        GetInstance()->GetDevice(),
        GetInstance()->GetSwapchain()
    );

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);
    }

    auto *frame = GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<renderer::Frame>();
    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

    PreFrameUpdate(frame);

    HYPERION_ASSERT_RESULT(frame->BeginCapture(GetInstance()->GetDevice()));

    m_world.Render(this, frame);

    game->OnFrameBegin(this, frame);

    if (auto *environment = render_state.GetScene().render_environment) {
        if (environment->IsReady()) {
            environment->RenderComponents(this, frame);
        }
    }

    RenderDeferred(frame);
    RenderFinalPass(frame);

    HYPERION_ASSERT_RESULT(frame->EndCapture(GetInstance()->GetDevice()));

    frame_result = frame->Submit(&GetInstance()->GetGraphicsQueue());

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);
    }

    game->OnFrameEnd(this, frame);

    GetInstance()->GetFrameHandler()->PresentFrame(&GetInstance()->GetGraphicsQueue(), GetInstance()->GetSwapchain());
    GetInstance()->GetFrameHandler()->NextFrame();
}

Handle<RendererInstance> Engine::FindOrCreateRendererInstance(const Handle<Shader> &shader, const RenderableAttributeSet &renderable_attributes)
{
    if (!shader) {
        DebugLog(
            LogType::Warn,
            "Shader is empty; Cannot create or find RendererInstance.\n"
        );

        return Handle<RendererInstance>::empty;
    }

    RenderableAttributeSet new_renderable_attributes(renderable_attributes);
    new_renderable_attributes.shader_id = shader->GetID();

    std::lock_guard guard(m_renderer_instance_mapping_mutex);

    const auto it = m_renderer_instance_mapping.Find(new_renderable_attributes);

    if (it != m_renderer_instance_mapping.End()) {
        return it->second.Lock();
    }

    auto &render_list_bucket = m_render_list_container.Get(new_renderable_attributes.material_attributes.bucket);

    // create a RendererInstance with the given params
    auto renderer_instance = CreateHandle<RendererInstance>(
        Handle<Shader>(shader),
        Handle<RenderPass>(render_list_bucket.GetRenderPass()),
        new_renderable_attributes
    );

    AddRendererInstanceInternal(renderer_instance);

    return renderer_instance;
}
    
Handle<RendererInstance> Engine::AddRendererInstance(std::unique_ptr<RendererInstance> &&_renderer_instance)
{
    auto renderer_instance = CreateHandle<RendererInstance>(_renderer_instance.release());
    
    std::lock_guard guard(m_renderer_instance_mapping_mutex);
    AddRendererInstanceInternal(renderer_instance);

    return renderer_instance;
}
    
void Engine::AddRendererInstanceInternal(Handle<RendererInstance> &renderer_instance)
{
    m_renderer_instance_mapping.Insert(
        renderer_instance->GetRenderableAttributes(),
        renderer_instance
    );

    m_render_list_container
        .Get(renderer_instance->GetRenderableAttributes().material_attributes.bucket)
        .AddRendererInstance(renderer_instance);
}

void Engine::PreFrameUpdate(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_render_list_container.AddPendingRendererInstances(this);
    
    if (auto num_enqueued = render_scheduler.NumEnqueued()) {
        render_scheduler.Flush([frame](RenderFunctor &fn) {
            HYPERION_ASSERT_RESULT(fn(frame->GetCommandBuffer(), frame->GetFrameIndex()));
        });
    }

    UpdateBuffersAndDescriptors(frame->GetFrameIndex());
    ResetRenderState(
        RENDER_STATE_VISIBILITY | RENDER_STATE_SCENE
    );
}

void Engine::ResetRenderState(RenderStateMask mask)
{
    render_state.Reset(mask);
}

void Engine::UpdateBuffersAndDescriptors(UInt frame_index)
{
    m_safe_deleter.PerformEnqueuedDeletions(this);

    shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->lights.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->shadow_maps.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->env_probes.UpdateBuffer(m_instance->GetDevice(), frame_index);

    m_instance->GetDescriptorPool().AddPendingDescriptorSets(m_instance->GetDevice(), frame_index);
    m_instance->GetDescriptorPool().DestroyPendingDescriptorSets(m_instance->GetDevice(), frame_index);
    m_instance->GetDescriptorPool().UpdateDescriptorSets(m_instance->GetDevice(), frame_index);
}

void Engine::RenderDeferred(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_deferred_renderer.Render(this, frame, render_state.GetScene().render_environment);
}

void Engine::RenderFinalPass(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *pipeline = m_root_pipeline->GetPipeline();
    const UInt acquired_image_index = m_instance->GetFrameHandler()->GetAcquiredImageIndex();

    m_root_pipeline->GetFramebuffers()[acquired_image_index]->BeginCapture(frame->GetCommandBuffer());
    
    pipeline->Bind(frame->GetCommandBuffer());

    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

#if HYP_FEATURES_ENABLE_RAYTRACING
    /* TMP */
    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );
#endif

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->Render(this, frame->GetCommandBuffer());
    
    m_root_pipeline->GetFramebuffers()[acquired_image_index]->EndCapture(frame->GetCommandBuffer());
}
} // namespace hyperion::v2
