/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

class Texture;
class TextureMipmapRenderer;

const FixedArray<Pair<Vec3f, Vec3f>, 6> Texture::cubemap_directions = {
    Pair<Vec3f, Vec3f> { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { -1, 0, 0 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 1, 0 }, Vec3f { 0, 0, -1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, -1, 0 }, Vec3f { 0, 0, 1 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, 1 }, Vec3f { 0, 1, 0 } },
    Pair<Vec3f, Vec3f> { Vec3f { 0, 0, -1 }, Vec3f { 0, 1, 0 } }
};

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture) : renderer::RenderCommand
{
    WeakHandle<Texture>     texture;
    renderer::ResourceState initial_state;
    ImageRef                image;
    ImageViewRef            image_view;

    RENDER_COMMAND(CreateTexture)(
        const WeakHandle<Texture> &texture,
        renderer::ResourceState initial_state,
        ImageRef image,
        ImageViewRef image_view
    ) : texture(texture),
        initial_state(initial_state),
        image(std::move(image)),
        image_view(std::move(image_view))
    {
        AssertThrow(this->image.IsValid());
        AssertThrow(this->image_view.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateTexture)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<Texture> texture_locked = texture.Lock()) {
            if (!image->IsCreated()) {
                HYPERION_BUBBLE_ERRORS(image->Create(g_engine->GetGPUDevice(), g_engine->GetGPUInstance(), initial_state));
            }

            if (!image_view->IsCreated()) {
                HYPERION_BUBBLE_ERRORS(image_view->Create(g_engine->GetGPUInstance()->GetDevice(), image.Get()));
            }
            
            if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
                g_engine->GetRenderData()->textures.AddResource(texture.GetID(), image_view);
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture) : renderer::RenderCommand
{
    WeakHandle<Texture> texture;

    RENDER_COMMAND(DestroyTexture)(const WeakHandle<Texture> &texture)
        : texture(texture)
    {
    }

    virtual ~RENDER_COMMAND(DestroyTexture)() override = default;

    virtual RendererResult operator()() override
    {
        // If shutting down, skip removing the resource here,
        // render data may have already been destroyed
        if (g_engine->IsShuttingDown()) {
            HYPERION_RETURN_OK;
        }

        if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            g_engine->GetRenderData()->textures.RemoveResource(texture.GetID());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateMipImageView) : renderer::RenderCommand
{
    ImageRef        src_image;
    ImageViewRef    mip_image_view;
    uint32          mip_level;

    RENDER_COMMAND(CreateMipImageView)(
        ImageRef src_image,
        ImageViewRef mip_image_view,
        uint32 mip_level
    ) : src_image(std::move(src_image)),
        mip_image_view(std::move(mip_image_view)),
        mip_level(mip_level)
    {
    }

    virtual RendererResult operator()() override
    {
        return mip_image_view->Create(
            g_engine->GetGPUDevice(),
            src_image.Get(),
            mip_level, 1,
            0, src_image->NumFaces()
        );
    }
};

struct RENDER_COMMAND(RenderTextureMipmapLevels) : renderer::RenderCommand
{
    ImageRef                    m_image;
    ImageViewRef                m_image_view;
    Array<ImageViewRef>         m_mip_image_views;
    Array<RC<FullScreenPass>>   m_passes;

    RENDER_COMMAND(RenderTextureMipmapLevels)(
        ImageRef image,
        ImageViewRef image_view,
        Array<ImageViewRef> mip_image_views,
        Array<RC<FullScreenPass>> passes
    ) : m_image(std::move(image)),
        m_image_view(std::move(image_view)),
        m_mip_image_views(std::move(mip_image_views)),
        m_passes(std::move(passes))
    {
        AssertThrow(m_image != nullptr);
        AssertThrow(m_image_view != nullptr);

        AssertThrow(m_passes.Size() == m_mip_image_views.Size());

        for (SizeType index = 0; index < m_mip_image_views.Size(); index++) {
            AssertThrow(m_mip_image_views[index] != nullptr);
            AssertThrow(m_passes[index] != nullptr);
        }
    }

    virtual RendererResult operator()() override
    {
        // draw a quad for each level
        renderer::SingleTimeCommands commands { g_engine->GetGPUDevice() };

        commands.Push([this](const CommandBufferRef &command_buffer)
        {
            const Vec3u extent = m_image->GetExtent();

            uint32 mip_width = extent.x,
                mip_height = extent.y;

            ImageRef &dst_image = m_image;

            for (uint32 mip_level = 0; mip_level < uint32(m_mip_image_views.Size()); mip_level++) {
                RC<FullScreenPass> &pass = m_passes[mip_level];
                AssertThrow(pass != nullptr);

                const uint32 prev_mip_width = mip_width,
                    prev_mip_height = mip_height;

                mip_width = MathUtil::Max(1u, extent.x >> (mip_level));
                mip_height = MathUtil::Max(1u, extent.y >> (mip_level));

                struct alignas(128)
                {
                    Vec4u   dimensions;
                    Vec4u   prev_dimensions;
                    uint32  mip_level;
                } push_constants;

                push_constants.dimensions = { mip_width, mip_height, 0, 0 };
                push_constants.prev_dimensions = { prev_mip_width, prev_mip_height, 0, 0 };
                push_constants.mip_level = mip_level;

                {
                    Frame temp_frame = Frame::TemporaryFrame(command_buffer);

                    pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
                    pass->Begin(&temp_frame);

                    pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind(
                        &temp_frame,
                        pass->GetRenderGroup()->GetPipeline(),
                        { }
                    );
                    
                    pass->GetQuadMesh()->GetRenderResource().Render(pass->GetCommandBuffer(temp_frame.GetFrameIndex()));
                    pass->End(&temp_frame);
                }

                const ImageRef &src_image = pass->GetAttachment(0)->GetImage();
                const ByteBuffer src_image_byte_buffer = src_image->ReadBack(g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

                // Blit into mip level
                dst_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::COPY_DST
                );

                src_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::COPY_SRC
                );

                HYPERION_BUBBLE_ERRORS(
                    dst_image->Blit(
                        command_buffer,
                        src_image.Get(),
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = src_image->GetExtent().x,
                            .y1 = src_image->GetExtent().y
                        },
                        Rect<uint32> {
                            .x0 = 0,
                            .y0 = 0,
                            .x1 = dst_image->GetExtent().x,
                            .y1 = dst_image->GetExtent().y
                        }
                    )
                );

                src_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::SHADER_RESOURCE
                );

                // put this mip into readable state
                dst_image->InsertSubResourceBarrier(
                    command_buffer,
                    renderer::ImageSubResource { .base_mip_level = mip_level },
                    renderer::ResourceState::SHADER_RESOURCE
                );
            }

            // all mip levels have been transitioned into this state
            dst_image->SetResourceState(
                renderer::ResourceState::SHADER_RESOURCE
            );

            HYPERION_RETURN_OK;
        });

        return commands.Execute();
    }
};

#pragma endregion Render commands

#pragma region TextureMipmapRenderer

class TextureMipmapRenderer // Come back to this for: UI rendering (caching each object as its own texture)
{
public:
    TextureMipmapRenderer(ImageRef image, ImageViewRef image_view)
        : m_image(std::move(image)),
          m_image_view(std::move(image_view))
    {
    }

    void Create()
    {
        const uint32 num_mip_levels = m_image->NumMipmaps();

        m_mip_image_views.Resize(num_mip_levels);
        m_passes.Resize(num_mip_levels);

        const Vec3u extent = m_image->GetExtent();

        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("GenerateMipmaps"),
            ShaderProperties()
        );

        for (uint32 mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

            DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

            const uint32 mip_width = MathUtil::Max(1u, extent.x >> mip_level);
            const uint32 mip_height = MathUtil::Max(1u, extent.y >> mip_level);

            ImageViewRef mip_image_view = MakeRenderObject<ImageView>();
            PUSH_RENDER_COMMAND(CreateMipImageView, m_image, mip_image_view, mip_level);

            const DescriptorSetRef &generate_mipmaps_descriptor_set = descriptor_table->GetDescriptorSet(NAME("GenerateMipmapsDescriptorSet"), 0);
            AssertThrow(generate_mipmaps_descriptor_set != nullptr);
            generate_mipmaps_descriptor_set->SetElement(NAME("InputTexture"), mip_level == 0 ? m_image_view : m_mip_image_views[mip_level - 1]);
            DeferCreate(descriptor_table, g_engine->GetGPUDevice());

            m_mip_image_views[mip_level] = std::move(mip_image_view);

            {
                RC<FullScreenPass> pass = MakeRefCountedPtr<FullScreenPass>(
                    shader,
                    descriptor_table,
                    m_image->GetTextureFormat(),
                    Vec2u { mip_width, mip_height }
                );

                pass->Create();

                m_passes[mip_level] = std::move(pass);
            }
        }
    }

    void Destroy()
    {
        m_passes.Clear();

        SafeRelease(std::move(m_image));
        SafeRelease(std::move(m_image_view));
        SafeRelease(std::move(m_mip_image_views));
    }

    void Render()
    {
        PUSH_RENDER_COMMAND(
            RenderTextureMipmapLevels,
            m_image,
            m_image_view,
            m_mip_image_views,
            m_passes
        );
    }

private:
    ImageRef                    m_image;
    ImageViewRef                m_image_view;
    Array<ImageViewRef>         m_mip_image_views;
    Array<RC<FullScreenPass>>   m_passes;
};

#pragma endregion TextureMipmapRenderer

#pragma region Texture

Texture::Texture()
    : Texture(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA8,
        Vec3u { 1, 1, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    })
{
}

Texture::Texture(const ImageRef &image, const ImageViewRef &image_view)
    : m_image(image),
      m_image_view(image_view)
{
}

Texture::Texture(const TextureDesc &texture_desc)
    : m_streamed_texture_data(MakeRefCountedPtr<StreamedTextureData>(TextureData { texture_desc })),
      m_image(MakeRenderObject<Image>(m_streamed_texture_data)),
      m_image_view(MakeRenderObject<ImageView>())
{
}

Texture::Texture(const RC<StreamedTextureData> &streamed_texture_data)
    : m_streamed_texture_data(streamed_texture_data),
      m_image(MakeRenderObject<Image>(streamed_texture_data)),
      m_image_view(MakeRenderObject<ImageView>())
{
}

Texture::~Texture()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));

    if (IsInitCalled()) {
        PUSH_RENDER_COMMAND(DestroyTexture, WeakHandleFromThis());
    }
}

void Texture::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        SafeRelease(std::move(m_image));
        SafeRelease(std::move(m_image_view));

        if (IsInitCalled()) {
            PUSH_RENDER_COMMAND(DestroyTexture, WeakHandleFromThis());
        }
    }));

    PUSH_RENDER_COMMAND(
        CreateTexture,
        WeakHandleFromThis(),
        renderer::ResourceState::SHADER_RESOURCE,
        m_image,
        m_image_view
    );

    SetReady(true);
}

void Texture::SetStreamedTextureData(const RC<StreamedTextureData> &streamed_texture_data)
{
    Mutex::Guard guard(m_readback_mutex);

    m_streamed_texture_data = streamed_texture_data;
}

void Texture::GenerateMipmaps()
{
    AssertReady();

    TextureMipmapRenderer mipmap_renderer(m_image, m_image_view);
    mipmap_renderer.Create();
    mipmap_renderer.Render();
    mipmap_renderer.Destroy();
}

void Texture::Readback()
{
    Mutex::Guard guard(m_readback_mutex);

    Readback_Internal();
}

void Texture::Readback_Internal()
{
    AssertReady();

    struct RENDER_COMMAND(Texture_Readback) : public renderer::RenderCommand
    {
        ImageRef    image;
        ByteBuffer  &result_byte_buffer;
        TextureDesc &texture_desc;

        RENDER_COMMAND(Texture_Readback)(const ImageRef &image, ByteBuffer &result_byte_buffer, TextureDesc &texture_desc)
            : image(image),
              result_byte_buffer(result_byte_buffer),
              texture_desc(texture_desc)
        {
        }

        virtual ~RENDER_COMMAND(Texture_Readback)() override
        {
            SafeRelease(std::move(image));
        }

        virtual RendererResult operator()() override
        {
            GPUBufferRef gpu_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
            HYPERION_ASSERT_RESULT(gpu_buffer->Create(g_engine->GetGPUDevice(), image->GetByteSize()));
            gpu_buffer->Map(g_engine->GetGPUDevice());
            gpu_buffer->SetResourceState(renderer::ResourceState::COPY_DST);

            renderer::SingleTimeCommands commands { g_engine->GetGPUDevice() };

            commands.Push([this, &gpu_buffer](const CommandBufferRef &command_buffer)
            {
                const renderer::ResourceState previous_resource_state = image->GetResourceState();

                image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

                gpu_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);
                image->CopyToBuffer(command_buffer, gpu_buffer);
                gpu_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

                image->InsertBarrier(command_buffer, previous_resource_state);

                HYPERION_RETURN_OK;
            });

            RendererResult result = commands.Execute();

            if (!result) {
                return result;
            }

            result_byte_buffer.SetSize(gpu_buffer->Size());
            gpu_buffer->Read(g_engine->GetGPUDevice(), result_byte_buffer.Size(), result_byte_buffer.Data());

            gpu_buffer->Destroy(g_engine->GetGPUDevice());

            texture_desc = image->GetTextureDesc();

            HYPERION_RETURN_OK;
        }
    };

    ByteBuffer result_byte_buffer;
    TextureDesc texture_desc;

    PUSH_RENDER_COMMAND(Texture_Readback, m_image, result_byte_buffer, texture_desc);
    HYP_SYNC_RENDER();

    const SizeType expected = m_image->GetByteSize();
    const SizeType real = result_byte_buffer.Size();

    AssertThrowMsg(expected == real, "Failed to readback texture: expected size: %llu, got %llu", expected, real);

    m_streamed_texture_data = MakeRefCountedPtr<StreamedTextureData>(TextureData {
        texture_desc,
        std::move(result_byte_buffer)
    });
}

Vec4f Texture::Sample(Vec3f uvw, uint32 face_index)
{
    if (!IsReady()) {
        HYP_LOG(Texture, Warning, "Texture is not ready, cannot sample");

        return Vec4f::Zero();
    }

    if (face_index >= NumFaces()) {
        HYP_LOG(Texture, Warning, "Face index out of bounds: {} >= {}", face_index, NumFaces());

        return Vec4f::Zero();
    }

    // keep reference alive in case m_streamed_texture_data changes outside of the mutex lock.
    RC<StreamedTextureData> streamed_texture_data;
    TResourceHandle<StreamedTextureData> resource_handle;

    {
        Mutex::Guard guard(m_readback_mutex);

        if (!m_streamed_texture_data) {
            HYP_LOG(Texture, Warning, "Texture does not have streamed data present, attempting readback...");

            Readback_Internal();

            if (!m_streamed_texture_data) {
                HYP_LOG(Texture, Warning, "Texture readback failed. Sample will return zero.");

                return Vec4f::Zero();
            }
        }

        resource_handle = TResourceHandle<StreamedTextureData>(*m_streamed_texture_data);

        if (resource_handle->GetTextureData().buffer.Size() == 0) {
            HYP_LOG(Texture, Warning, "Texture buffer is empty; forcing readback.");

            resource_handle.Reset();

            Readback_Internal();

            if (!m_streamed_texture_data) {
                HYP_LOG(Texture, Warning, "Texture readback failed. Sample will return zero.");

                return Vec4f::Zero();
            }

            resource_handle = TResourceHandle<StreamedTextureData>(*m_streamed_texture_data);

            if (resource_handle->GetTextureData().buffer.Size() == 0) {
                HYP_LOG(Texture, Warning, "Texture buffer is still empty after readback; sample will return zero.");

                return Vec4f::Zero();
            }
        }

        streamed_texture_data = m_streamed_texture_data;
    }

    const TextureData *texture_data = &resource_handle->GetTextureData();

    const Vec3u coord = {
        uint32(uvw.x * float(texture_data->desc.extent.x - 1) + 0.5f),
        uint32(uvw.y * float(texture_data->desc.extent.y - 1) + 0.5f),
        uint32(uvw.z * float(texture_data->desc.extent.z - 1) + 0.5f)
    };

    const uint32 bytes_per_pixel = renderer::NumBytes(texture_data->desc.format);

    if (bytes_per_pixel != 1) {
        HYP_LOG(Texture, Warning, "Unsupported bytes per pixel to use with Sample(): {}", bytes_per_pixel);

        HYP_BREAKPOINT;

        return Vec4f::Zero();
    }

    const uint32 num_components = renderer::NumComponents(texture_data->desc.format);

    const uint32 index = face_index * (texture_data->desc.extent.x * texture_data->desc.extent.y * texture_data->desc.extent.z * bytes_per_pixel * num_components)
        + coord.z * (texture_data->desc.extent.x * texture_data->desc.extent.y * bytes_per_pixel * num_components)
        + coord.y * (texture_data->desc.extent.x * bytes_per_pixel * num_components)
        + coord.x * bytes_per_pixel * num_components;

    if (index >= texture_data->buffer.Size()) {
        HYP_LOG(Texture, Warning, "Index out of bounds, index: {}, buffer size: {}, coord: {}, dimensions: {}, num faces: {}", index, texture_data->buffer.Size(),
            coord, texture_data->desc.extent, NumFaces());

        return Vec4f::Zero();
    }

    const ubyte *data = texture_data->buffer.Data() + index;

    switch (num_components) {
    case 1:
        return Vec4f(float(data[0]) / 255.0f);
    case 2:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, 0.0f, 1.0f);
    case 3:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, 1.0f);
    case 4:
        return Vec4f(float(data[0]) / 255.0f, float(data[1]) / 255.0f, float(data[2]) / 255.0f, float(data[3]) / 255.0f);
    default: // should never happen
        HYP_LOG(Texture, Error, "Unsupported number of components: {}", num_components);

        return Vec4f::Zero();
    }
}

Vec4f Texture::Sample2D(Vec2f uv)
{
    if (GetType() != ImageType::TEXTURE_TYPE_2D) {
        HYP_LOG(Texture, Warning, "Unsupported texture type to use with Sample2D(): {}", GetType());

        return Vec4f::Zero();
    }

    return Sample(Vec3f { uv.x, uv.y, 0.0f }, 0);
}

/// https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/5337472/
Vec4f Texture::SampleCube(Vec3f direction)
{
    if (GetType() != ImageType::TEXTURE_TYPE_CUBEMAP) {
        HYP_LOG(Texture, Warning, "Unsupported texture type to use with SampleCube(): {}", GetType());

        return Vec4f::Zero();
    }

    Vec3f abs_dir = MathUtil::Abs(direction);
    uint32 face_index = 0;

    float mag;
    Vec2f uv;

    if (abs_dir.z >= abs_dir.x && abs_dir.z >= abs_dir.y) {
        mag = abs_dir.z;

        if (direction.z < 0.0f) {
            face_index = 5;
            uv = Vec2f(-direction.x, -direction.y);
        } else {
            face_index = 4;
            uv = Vec2f(direction.x, -direction.y);
        }
    } else if (abs_dir.y >= abs_dir.x) {
        mag = abs_dir.y;

        if (direction.y < 0.0f) {
            face_index = 3;
            uv = Vec2f(direction.x, -direction.z);
        } else {
            face_index = 2;
            uv = Vec2f(direction.x, direction.z);
        }
    } else {
        mag = abs_dir.x;

        if (direction.x < 0.0f) {
            face_index = 1;
            uv = Vec2f(direction.z, -direction.y);
        } else {
            face_index = 0;
            uv = Vec2f(-direction.z, -direction.y);
        }
    }

    return Sample(Vec3f { uv / mag * 0.5f + 0.5f, 0.0f }, face_index);
}

#pragma endregion Texture

} // namespace hyperion
