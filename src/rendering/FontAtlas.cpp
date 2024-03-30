#include <rendering/FontAtlas.hpp>
#include <rendering/backend/RenderCommand.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Rect;
using renderer::StagingBuffer;

#pragma region Render commands

struct RENDER_COMMAND(RenderFontAtlas) : renderer::RenderCommand {
    RENDER_COMMAND(RenderFontAtlas)(const Handle<Texture> &atlas, const Handle<Texture> &glyph, const Extent2D location)
        : atlas(atlas),
          glyph(glyph),
          location(location)
    {
    }

    virtual ~RENDER_COMMAND(RenderFontAtlas)() = default;

    Handle<Texture> atlas;
    Handle<Texture> glyph;

    Extent2D location;
};

#pragma endregion

void FontAtlas::RenderCharacter(Vec2i location, Extent2D dimensions, Glyph &glyph)
{
    Threads::AssertOnThread(THREAD_RENDER);
    auto commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

    auto &image = glyph.GetTexture()->GetImage();

    HYP_SYNC_RENDER();

    Rect src_rect = {
        0, 0,
        image->GetExtent().width,
        image->GetExtent().height
    };

    Rect dest_rect = {
        uint32(location.x),
        uint32(location.y),
        uint32(location.x + image->GetExtent()[0]),
        uint32(location.y + image->GetExtent()[1])
    };

    commands.Push([&](CommandBuffer *command_buffer)
    {
        // put src image in state for copying from
        image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        // put dst image in state for copying to
        m_atlas->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        //m_buffer->CopyFrom(command_buffer, staging_buffer, sizeof(value));
        m_atlas->GetImage()->Blit(command_buffer, image, src_rect, dest_rect);


        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(Engine::Get()->GetGPUInstance()->GetDevice()));

    //PUSH_RENDER_COMMAND(RenderFontAtlas, m_atlas, glyph.GetTexture(), location);
}

renderer::Result FontAtlas::WriteToBuffer(const RC<ByteBuffer> &buffer)
{
    const SizeType buffer_size = m_atlas_dimensions.width * m_atlas_dimensions.height;
    renderer::Result result = renderer::Result::OK;

    const auto gpu_device = Engine::Get()->GetGPUDevice();
    buffer->SetSize(buffer_size);

    StagingBuffer texture_staging_buffer;
    HYPERION_BUBBLE_ERRORS(texture_staging_buffer.Create(gpu_device, buffer_size));
    texture_staging_buffer.Memset(gpu_device, buffer_size, 0xAA);

    if (!result) {
        HYPERION_IGNORE_ERRORS(texture_staging_buffer.Destroy(gpu_device));

        return result;
    }

    auto commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

    AssertThrow(m_atlas);
    AssertThrow(m_atlas->GetImage());
    AssertThrow(m_atlas->GetImage()->GetGPUImage());

    commands.Push([&](CommandBuffer *cmd)
    {

        // put src image in state for copying from
        m_atlas->GetImage()->GetGPUImage()->InsertBarrier(cmd, renderer::ResourceState::COPY_SRC);
        // put dst image in state for copying to
        //m_atlas->GetImage().GetGPUImage()->InsertBarrier(cmd, renderer::ResourceState::COPY_DST);

        //m_atlas->GetImage()->GetGPUImage()->Read(gpu_device, buffer_size, image_data.Data());

        m_atlas->GetImage()->CopyToBuffer(cmd, &texture_staging_buffer);

        HYPERION_RETURN_OK;
    });

    HYPERION_PASS_ERRORS(commands.Execute(gpu_device), result);
    texture_staging_buffer.Read(Engine::Get()->GetGPUInstance()->GetDevice(), buffer_size, buffer->Data());
    HYPERION_PASS_ERRORS(texture_staging_buffer.Destroy(gpu_device), result);

    HYPERION_RETURN_OK;
}



}; // namespace hyperion::v2

