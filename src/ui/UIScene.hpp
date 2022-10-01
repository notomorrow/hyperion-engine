#ifndef HYPERION_V2_UI_SCENE_H
#define HYPERION_V2_UI_SCENE_H

#include <core/Base.hpp>
#include <core/Containers.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <scene/Scene.hpp>

#include <core/Containers.hpp>
#include <GameCounter.hpp>

#include <vector>

namespace hyperion::v2 {

using renderer::Extent2D;

class UIObject : public EngineComponentBase<STUB_CLASS(UIObject)>
{
public:
    UIObject() : m_position { }, m_dimensions { } {}
    virtual ~UIObject() = default;

    const Extent2D &GetPosition() const
        { return m_position; }

    const Extent2D &GetExtent() const
        { return m_dimensions; }

protected:
    Extent2D m_position,
        m_dimensions;
};

class UIScene : public EngineComponentBase<STUB_CLASS(UIScene)>
{
public:
    UIScene();
    UIScene(const UIScene &other) = delete;
    UIScene &operator=(const UIScene &other) = delete;
    ~UIScene();

    Handle<Scene> &GetScene()
        { return m_scene; }

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    // bool Add(Handle<UIObject> &&ui_object);
    // bool Add(const Handle<UIObject> &ui_object);
    // bool Remove(const UIObject::ID &id);

private:
    Handle<Scene> m_scene;
    // FlatMap<UIObject::ID, Handle<UIObject>> m_ui_objects;
};

} // namespace hyperion::v2

#endif