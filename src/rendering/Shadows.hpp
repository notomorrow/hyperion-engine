#ifndef HYPERION_V2_SHADOWS_H
#define HYPERION_V2_SHADOWS_H

#include <rendering/FullScreenPass.hpp>
#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/Compute.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::DescriptorSet;

enum class ShadowMode
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM
};

struct RenderCommand_CreateShadowMapDescriptors;

class ShadowPass : public FullScreenPass
{
    friend struct RenderCommand_CreateShadowMapDescriptors;

public:
    ShadowPass();
    ShadowPass(const ShadowPass &other) = delete;
    ShadowPass &operator=(const ShadowPass &other) = delete;
    virtual ~ShadowPass();

    Handle<Scene> &GetScene() { return m_scene; }
    const Handle<Scene> &GetScene() const { return m_scene; }

    Handle<Light> &GetLight() { return m_light; }
    const Handle<Light> &GetLight() const { return m_light; }

    void SetLight(Handle<Light> &&light)
    {
        m_light = std::move(light);

        if (m_light != nullptr) {
            m_light->SetShadowMapIndex(m_shadow_map_index);
        }
    }

    void SetParentScene(Scene::ID id);

    ShadowMode GetShadowMode() const { return m_shadow_mode; }
    void SetShadowMode(ShadowMode shadow_mode) { m_shadow_mode = shadow_mode; }

    const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin) { m_origin = origin; }

    float GetMaxDistance() const { return m_max_distance; }
    void SetMaxDistance(float max_distance) { m_max_distance = max_distance; }

    BoundingBox GetAABB() const
    {
        return {
            MathUtil::Round(m_origin - m_max_distance),
            MathUtil::Round(m_origin + m_max_distance)
        };
    }

    UInt GetShadowMapIndex() const
        { return m_shadow_map_index; }

    void SetShadowMapIndex(UInt index)
    {
        m_shadow_map_index = index;

        if (m_light != nullptr) {
            m_light->SetShadowMapIndex(index);
        }
    }

    ImageView *GetShadowMap(UInt frame_index) const
    {
        if (auto framebuffer = m_framebuffers[frame_index]) {
            if (!framebuffer->GetFramebuffer().GetAttachmentRefs().empty()) {
                if (auto *attachment_ref = framebuffer->GetFramebuffer().GetAttachmentRefs().front()) {
                    return attachment_ref->GetImageView();
                }
            }
        }

        return nullptr;
    }

    void CreateShader(Engine *engine);
    void CreateRendererInstance(Engine *engine);
    virtual void CreateRenderPass(Engine *engine) override;
    virtual void CreateDescriptors(Engine *engine) override;

    virtual void Create(Engine *engine) override;
    virtual void Destroy(Engine *engine) override;
    virtual void Render(Engine *engine, Frame *frame) override;

private:
    void CreateShadowMap(Engine *engine);
    void CreateComputePipelines(Engine *engine);

    ShadowMode m_shadow_mode;
    Handle<Scene> m_scene;
    Handle<Light> m_light;
    Scene::ID m_parent_scene_id;
    Vector3 m_origin;
    Float m_max_distance;
    UInt m_shadow_map_index;
    Extent2D m_dimensions;

    std::unique_ptr<Image> m_shadow_map_image;
    std::unique_ptr<ImageView> m_shadow_map_image_view;
    Handle<ComputePipeline> m_blur_shadow_map;
    FixedArray<DescriptorSet, 2> m_blur_descriptor_sets;
};

class ShadowRenderer
    : public EngineComponentBase<STUB_CLASS(ShadowRenderer)>,
      public RenderComponent<ShadowRenderer>
{
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_SHADOWS;

    ShadowRenderer(Handle<Light> &&light);
    ShadowRenderer(Handle<Light> &&light, const Vector3 &origin, float max_distance);
    ShadowRenderer(Handle<Light> &&light, const BoundingBox &aabb);
    ShadowRenderer(const ShadowRenderer &other) = delete;
    ShadowRenderer &operator=(const ShadowRenderer &other) = delete;
    virtual ~ShadowRenderer();

    ShadowPass &GetPass() { return m_shadow_pass; }
    const ShadowPass &GetPass() const { return m_shadow_pass; }

    ShadowMode GetShadowMode() const { return m_shadow_pass.GetShadowMode(); }
    void SetShadowMode(ShadowMode shadow_mode) { m_shadow_pass.SetShadowMode(shadow_mode); }

    const Vector3 &GetOrigin() const { return m_shadow_pass.GetOrigin(); }
    void SetOrigin(const Vector3 &origin) { m_shadow_pass.SetOrigin(origin); }

    void SetParentScene(const Handle<Scene> &parent_scene)
    {
        if (parent_scene != nullptr) {
            m_shadow_pass.SetParentScene(parent_scene->GetID());
        } else {
            m_shadow_pass.SetParentScene(Scene::empty_id);
        }
    }

    void Init(Engine *engine);     // init on render thread
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    void UpdateSceneCamera(Engine *engine);

    virtual void OnEntityAdded(Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(Handle<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Handle<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    ShadowPass m_shadow_pass;
};


} // namespace hyperion::v2

#endif
