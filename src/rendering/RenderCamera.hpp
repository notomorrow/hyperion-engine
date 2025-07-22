/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector4.hpp>
#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Camera;
struct RenderSetup;

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
