#pragma once
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */


#include <rendering/RenderResource.hpp>

#include <rendering/RenderObject.hpp>

#include <core/math/Vector4.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Scene;
class RenderEnvironment;
class RenderCamera;

class RenderScene final : public RenderResourceBase
{
public:
    RenderScene(Scene* scene);
    virtual ~RenderScene() override;

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    Scene* m_scene;

    ImageRef m_shadowsTextureArrayImage;
    ImageViewRef m_shadowsTextureArrayImageView;
};

template <>
struct ResourceMemoryPoolInitInfo<RenderScene> : MemoryPoolInitInfo<RenderScene>
{
    static constexpr uint32 numElementsPerBlock = 8;
    static constexpr uint32 numInitialElements = 8;
};

} // namespace hyperion

