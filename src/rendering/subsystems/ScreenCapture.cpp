/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/subsystems/ScreenCapture.hpp>
#include <rendering/FinalPass.hpp>

#include <Engine.hpp>

namespace hyperion {

ScreenCaptureRenderSubsystem::ScreenCaptureRenderSubsystem(Name name, const Vec2u &window_size, ScreenCaptureMode screen_capture_mode)
    : RenderSubsystem(name),
      m_window_size(window_size),
      m_texture(CreateObject<Texture>(TextureDesc {
          ImageType::TEXTURE_TYPE_2D,
          InternalFormat::RGBA8_SRGB,
          Vec3u { window_size.x, window_size.y, 1 },
          FilterMode::TEXTURE_FILTER_NEAREST,
          FilterMode::TEXTURE_FILTER_NEAREST,
          WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
      })),
      m_screen_capture_mode(screen_capture_mode)
{
}

ScreenCaptureRenderSubsystem::~ScreenCaptureRenderSubsystem()
{
    SafeRelease(std::move(m_buffer));
}

void ScreenCaptureRenderSubsystem::Init()
{
    InitObject(m_texture);

    m_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
    HYPERION_ASSERT_RESULT(m_buffer->Create(g_engine->GetGPUDevice(), m_texture->GetImage()->GetByteSize()));
    m_buffer->SetResourceState(renderer::ResourceState::COPY_DST);
}

void ScreenCaptureRenderSubsystem::InitGame()
{
    
}

void ScreenCaptureRenderSubsystem::OnRemoved()
{
    SafeRelease(std::move(m_buffer));
    g_safe_deleter->SafeRelease(std::move(m_texture));
}

void ScreenCaptureRenderSubsystem::OnUpdate(GameCounter::TickUnit delta)
{
    // Do nothing
}

void ScreenCaptureRenderSubsystem::OnRender(Frame *frame)
{
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    FinalPass *final_pass = g_engine->GetFinalPass();
    AssertThrow(final_pass != nullptr);

    AssertThrow(m_texture.IsValid());
    AssertThrow(m_texture->IsReady());

    const ImageRef &image_ref = final_pass->GetLastFrameImage();
    AssertThrow(image_ref.IsValid());

    image_ref->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

    switch (m_screen_capture_mode) {
    case ScreenCaptureMode::TO_TEXTURE:
        m_texture->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        m_texture->GetImage()->Blit(
            command_buffer,
            image_ref
        );

        m_texture->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

        break;
    case ScreenCaptureMode::TO_BUFFER:
        AssertThrow(m_buffer.IsValid() && m_buffer->Size() >= image_ref->GetByteSize());

        m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        image_ref->CopyToBuffer(
            command_buffer,
            m_buffer
        );
    
        m_buffer->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);

        break;
    default:
        HYP_THROW("Invalid screen capture mode");
    }
}

} // namespace hyperion