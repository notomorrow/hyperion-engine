/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_CAMERA_HPP
#define HYPERION_RENDERING_CAMERA_HPP

#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Camera;
struct RenderSetup;

struct CameraShaderData
{
    Matrix4 view;
    Matrix4 projection;
    Matrix4 previousView;

    Vec4u dimensions;
    Vec4f cameraPosition;
    Vec4f cameraDirection;
    Vec4f jitter;

    float cameraNear;
    float cameraFar;
    float cameraFov;
    uint32 id;

    Vec4f _pad1;
    Vec4f _pad2;
    Vec4f _pad3;

    Matrix4 _pad4;
    Matrix4 _pad5;
    Matrix4 _pad6;
};

class RenderCamera final : public RenderResourceBase
{
public:
    RenderCamera(Camera* camera);
    virtual ~RenderCamera() override;

    HYP_FORCE_INLINE Camera* GetCamera() const
    {
        return m_camera;
    }

    void SetBufferData(const CameraShaderData& bufferData);

    /*! \note This method is only safe to call from the render thread. */
    HYP_FORCE_INLINE const CameraShaderData& GetBufferData() const
    {
        return m_bufferData;
    }

    void ApplyJitter(const RenderSetup& renderSetup);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void UpdateBufferData();

    Camera* m_camera;
    CameraShaderData m_bufferData;
};

} // namespace hyperion

#endif
