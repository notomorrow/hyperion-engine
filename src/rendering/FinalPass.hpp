/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FINAL_PASS_HPP
#define HYPERION_FINAL_PASS_HPP

#include <Config.hpp>

#include <rendering/SafeDeleter.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class ShaderManagerSystem;

extern ShaderManagerSystem *g_shader_manager;

struct RENDER_COMMAND(SetUITexture);

class FinalPass;

// Performs tonemapping, samples last postfx in chain
class CompositePass : public FullScreenPass
{
public:
    CompositePass();
    CompositePass(const CompositePass &other)               = delete;
    CompositePass &operator=(const CompositePass &other)    = delete;
    virtual ~CompositePass() override;

    void CreateShader();
    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;
};

class FinalPass : public FullScreenPass
{
public:
    friend struct RENDER_COMMAND(SetUITexture);

    FinalPass();
    FinalPass(const FinalPass &other)               = delete;
    FinalPass &operator=(const FinalPass &other)    = delete;
    virtual ~FinalPass() override;

    HYP_FORCE_INLINE CompositePass &GetCompositePass()
        { return m_composite_pass; }

    HYP_FORCE_INLINE const CompositePass &GetCompositePass() const
        { return m_composite_pass; }

    HYP_FORCE_INLINE const Handle<Texture> &GetUITexture() const
        { return m_ui_texture; }
    
    HYP_FORCE_INLINE void SetUITexture(Handle<Texture> texture);

    HYP_FORCE_INLINE const ImageRef &GetLastFrameImage() const
        { return m_last_frame_image; }

    virtual void Create() override;

    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
    void RenderUITexture() const;

    virtual void Resize_Internal(Extent2D new_size) override;

    CompositePass               m_composite_pass;
    ImageRef                    m_last_frame_image;

    // For UI blitting to screen
    Handle<Texture>             m_ui_texture;
    UniquePtr<FullScreenPass>   m_render_texture_to_screen_pass;
    uint8                       m_dirty_frame_indices;
};
} // namespace hyperion

#endif