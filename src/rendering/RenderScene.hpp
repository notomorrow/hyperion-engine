/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SCENE_HPP
#define HYPERION_RENDERING_SCENE_HPP

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Vector4.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Scene;
class RenderEnvironment;
class CameraRenderResource;

struct SceneShaderData
{
    Vec4f   _pad0;//aabb_max;
    Vec4f   _pad1;//aabb_min;
    Vec4f   fog_params;

    float   game_time;
    uint32  frame_counter;
    uint32  enabled_render_subsystems_mask;
    uint32  enabled_environment_maps_mask;

    HYP_PAD_STRUCT_HERE(uint8, 64 + 128);
};

static_assert(sizeof(SceneShaderData) == 256);

static constexpr uint32 max_scenes = (32ull * 1024ull) / sizeof(SceneShaderData);

class SceneRenderResource final : public RenderResourceBase
{
public:
    SceneRenderResource(Scene *scene);
    virtual ~SceneRenderResource() override;

    HYP_FORCE_INLINE Scene *GetScene() const
        { return m_scene; }

    HYP_FORCE_INLINE const TResourceHandle<CameraRenderResource> &GetCameraRenderResourceHandle() const
        { return m_camera_render_resource_handle; }

    void SetCameraRenderResourceHandle(const TResourceHandle<CameraRenderResource> &camera_render_resource_handle);

    HYP_FORCE_INLINE const Handle<RenderEnvironment> &GetEnvironment() const
        { return m_environment; }

    void SetBufferData(const SceneShaderData &buffer_data);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const SceneShaderData &GetBufferData() const
        { return m_buffer_data; }

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    Scene                                   *m_scene;

    TResourceHandle<CameraRenderResource>   m_camera_render_resource_handle;

    Handle<RenderEnvironment>               m_environment;

    SceneShaderData                         m_buffer_data;

    ImageRef                                m_shadows_texture_array_image;
    ImageViewRef                            m_shadows_texture_array_image_view;
};

template <>
struct ResourceMemoryPoolInitInfo<SceneRenderResource> : MemoryPoolInitInfo
{
    static constexpr uint32 num_elements_per_block = 16;
    static constexpr uint32 num_initial_elements = 16;
};

} // namespace hyperion

#endif
