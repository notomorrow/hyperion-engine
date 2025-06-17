/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FINAL_PASS_HPP
#define HYPERION_FINAL_PASS_HPP

#include <Config.hpp>

#include <core/Handle.hpp>

#include <rendering/SafeDeleter.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class ShaderManager;
class FullScreenPass;
class RenderWorld;
class Mesh;

struct RENDER_COMMAND(SetUILayerImageView);

class FinalPass final
{
public:
    friend struct RENDER_COMMAND(SetUILayerImageView);

    FinalPass(const SwapchainRef& swapchain);
    FinalPass(const FinalPass& other) = delete;
    FinalPass& operator=(const FinalPass& other) = delete;
    ~FinalPass();

    void SetUILayerImageView(const ImageViewRef& imageView);

    void Create();
    void Render(FrameBase* frame, RenderWorld* renderWorld);

private:
    SwapchainRef m_swapchain;
    Vec2u m_extent;
    TextureFormat m_imageFormat;
    Handle<Mesh> m_quadMesh;
    ImageViewRef m_uiLayerImageView;
    UniquePtr<FullScreenPass> m_renderTextureToScreenPass;
    uint8 m_dirtyFrameIndices;
};
} // namespace hyperion

#endif