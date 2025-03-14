/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_CAMERA_HPP
#define HYPERION_RENDERING_CAMERA_HPP

#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Camera;

struct alignas(256) CameraShaderData
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;

    Vec4u       dimensions;
    Vec4f       camera_position;
    Vec4f       camera_direction;
    Vec4f       jitter;
    
    float       camera_near;
    float       camera_far;
    float       camera_fov;
    uint32      id;
};

static_assert(sizeof(CameraShaderData) == 512);

static constexpr uint32 max_cameras = (16ull * 1024ull) / sizeof(CameraShaderData);

class CameraRenderResource final : public RenderResourceBase
{
public:
    CameraRenderResource(Camera *camera);
    CameraRenderResource(CameraRenderResource &&other) noexcept;
    virtual ~CameraRenderResource() override;

    HYP_FORCE_INLINE Camera *GetCamera() const
        { return m_camera; }

    void SetBufferData(const CameraShaderData &buffer_data);

    /*! \note This method is only safe to call from the render thread. */
    HYP_FORCE_INLINE const CameraShaderData &GetBufferData() const
        { return m_buffer_data; }

    void ApplyJitter();

    void EnqueueBind();
    void EnqueueUnbind();

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    Camera              *m_camera;
    CameraShaderData    m_buffer_data;
};

} // namespace hyperion

#endif
