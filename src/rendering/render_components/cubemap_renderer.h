#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include "../base.h"
#include "../post_fx.h"
#include "../renderer.h"
#include "../light.h"
#include "../render_component.h"

#include <rendering/backend/renderer_frame.h>

#include <math/bounding_box.h>
#include <scene/scene.h>
#include <camera/camera.h>
#include <types.h>

namespace hyperion::v2 {

using renderer::Frame;

class CubemapRendererPass : public FullScreenPass {
public:
    CubemapRendererPass();
    CubemapRendererPass(const CubemapRendererPass &other) = delete;
    CubemapRendererPass &operator=(const CubemapRendererPass &other) = delete;
    ~CubemapRendererPass();

    Ref<Scene> &GetScene()                  { return m_scene; }
    const Ref<Scene> &GetScene() const      { return m_scene; }

    void SetParentScene(Scene::ID id);

    const Vector3 &GetOrigin() const        { return m_origin; }
    void SetOrigin(const Vector3 &origin)   { m_origin = origin; }

    float GetMaxDistance() const            { return m_max_distance; }
    void SetMaxDistance(float max_distance) { m_max_distance = max_distance; }

    BoundingBox GetAabb() const
    {
        return {
            MathUtil::Round(m_origin - m_max_distance * 0.5f),
            MathUtil::Round(m_origin + m_max_distance * 0.5f)
        };
    }

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreatePipeline(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    Ref<Scene>                                               m_scene;
    Scene::ID                                                m_parent_scene_id;
    Vector3                                                  m_origin;
    float                                                    m_max_distance;
};

class CubemapRenderer : public EngineComponentBase<STUB_CLASS(CubemapRenderer)>, public RenderComponent<CubemapRenderer> {
public:
    CubemapRenderer();
    CubemapRenderer(const Vector3 &origin, float max_distance);
    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    virtual ~CubemapRenderer();

    const Ref<Scene> &GetScene() const    { return m_pass.GetScene(); }

    const Vector3 &GetOrigin() const      { return m_pass.GetOrigin(); }
    void SetOrigin(const Vector3 &origin) { m_pass.SetOrigin(origin); }

    void SetParentScene(const Ref<Scene> &parent_scene)
    {
        if (parent_scene != nullptr) {
            m_pass.SetParentScene(parent_scene->GetId());
        } else {
            m_pass.SetParentScene(Scene::empty_id);
        }
    }

    void Init(Engine *engine);

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    void UpdateSceneCamera(Engine *engine);
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    CubemapRendererPass m_pass;
};


} // namespace hyperion::v2

#endif
