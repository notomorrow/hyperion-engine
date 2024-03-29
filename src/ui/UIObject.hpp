#ifndef HYPERION_V2_UI_OBJECT_HPP
#define HYPERION_V2_UI_OBJECT_HPP

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/Delegate.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>

#include <math/Transform.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class UIScene;

// Helper function to get the scene from a UI scene
template <class UISceneType>
static inline Scene *GetScene(UISceneType *ui_scene)
{
    AssertThrow(ui_scene != nullptr);

    return ui_scene->GetScene().Get();
}

// UIObject

enum UIObjectAlignment : uint32
{
    UI_OBJECT_ALIGNMENT_TOP_LEFT        = 0,
    UI_OBJECT_ALIGNMENT_TOP_RIGHT       = 1,

    UI_OBJECT_ALIGNMENT_CENTER          = 2,

    UI_OBJECT_ALIGNMENT_BOTTOM_LEFT     = 3,
    UI_OBJECT_ALIGNMENT_BOTTOM_RIGHT    = 4
};

class UIObject : public EnableRefCountedPtrFromThis<UIObject>
{
public:
    UIObject(ID<Entity> entity, UIScene *parent);
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    virtual ~UIObject();

    virtual void Init();

    ID<Entity> GetEntity() const
        { return m_entity; }

    UIScene *GetParent() const
        { return m_parent; }

    Name GetName() const;
    void SetName(Name name);

    Vec2i GetPosition() const;
    void SetPosition(Vec2i position);

    Vec2i GetSize() const;
    void SetSize(Vec2i size);

    UIObjectAlignment GetAlignment() const;
    void SetAlignment(UIObjectAlignment alignment);

    Delegate<bool, const UIMouseEventData &>    OnMouseDown;
    Delegate<bool, const UIMouseEventData &>    OnMouseUp;
    Delegate<bool, const UIMouseEventData &>    OnMouseDrag;
    Delegate<bool, const UIMouseEventData &>    OnMouseHover;
    Delegate<bool, const UIMouseEventData &>    OnClick;

protected:
    virtual Handle<Material> GetMaterial() const;

    void UpdatePosition();
    void UpdateSize();

    ID<Entity>          m_entity;
    UIScene             *m_parent;

    Name                m_name;

    Vec2i               m_position;
    Vec2i               m_size;

    UIObjectAlignment   m_alignment;
};

// UIObjectProxy<T>

template <class T>
class UIObjectProxy
{
public:
    UIObjectProxy(NodeProxy node_proxy)
        : m_node_proxy(std::move(node_proxy))
    {
    }

    UIObjectProxy(const UIObjectProxy &other)                   = default;
    UIObjectProxy &operator=(const UIObjectProxy &other)        = default;
    UIObjectProxy(UIObjectProxy &&other) noexcept               = default;
    UIObjectProxy &operator=(UIObjectProxy &&other) noexcept    = default;
    ~UIObjectProxy()                                            = default;

    const NodeProxy &GetNode() const
        { return m_node_proxy; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return m_node_proxy.GetEntity().IsValid(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return m_node_proxy.GetEntity().IsValid(); }

    [[nodiscard]]
    T *Get() const
    {
        if (!IsValid()) {
            return nullptr;
        }

        Scene *scene = m_node_proxy.Get()->GetScene();
        AssertThrow(scene != nullptr);

        UIComponent *ui_component = scene->GetEntityManager()->TryGetComponent<UIComponent>(m_node_proxy.GetEntity());
        AssertThrow(ui_component != nullptr && ui_component->ui_object != nullptr);
        AssertThrow(ui_component->ui_object.Is<T>());

        return static_cast<T *>(ui_component->ui_object.Get());
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T *operator->() const
        { return EnsureValidPointer(Get()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T &operator*() const
        { return *EnsureValidPointer(Get()); }

private:
    NodeProxy   m_node_proxy;
};

} // namespace hyperion::v2

#endif