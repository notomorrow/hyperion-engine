/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_STAGE_HPP
#define HYPERION_UI_STAGE_HPP

#include <ui/UIObject.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <scene/Node.hpp>
#include <scene/Scene.hpp>

#include <scene/ecs/components/UIComponent.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace sys {
class SystemEvent;
} // namespace sys

using sys::SystemEvent;

class InputManager;

} // namespace hyperion

namespace hyperion {

class UIButton;
class FontAtlas;

struct UIObjectPressedState
{
    EnumFlags<MouseButtonState> mouseButtons = MouseButtonState::NONE;
    float heldTime = 0.0f;
};

enum class UIRayTestFlags : uint32
{
    NONE = 0x0,
    ONLY_VISIBLE = 0x1,

    DEFAULT = ONLY_VISIBLE
};

HYP_MAKE_ENUM_FLAGS(UIRayTestFlags)

/*! \brief The UIStage is the root of the UI scene graph. */

HYP_CLASS()
class HYP_API UIStage : public UIObject
{
    HYP_OBJECT_BODY(UIStage);

public:
    friend class UIObject;

    // The minimum and maximum depth values for the UI scene for layering
    static const int g_minDepth = -10000;
    static const int g_maxDepth = 10000;

    UIStage();
    UIStage(ThreadId ownerThreadId);
    UIStage(const UIStage& other) = delete;
    UIStage& operator=(const UIStage& other) = delete;
    virtual ~UIStage() override;

    /*! \brief Get the size of the surface that the UI objects are rendered on.
     *
     *  \return The size of the surface. */
    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetSurfaceSize() const
    {
        return m_surfaceSize;
    }

    HYP_METHOD()
    void SetSurfaceSize(Vec2i surfaceSize);

    /*! \brief Get the scene that contains the UI objects.
     *
     *  \return Handle to the scene. */
    HYP_METHOD()
    virtual Scene* GetScene() const override;

    /*! \brief Set the scene for this UIStage.
     *  \internal Used internally, for serialization.
     *
     *  \param scene The scene to set. */
    HYP_METHOD()
    void SetScene(const Handle<Scene>& scene);

    HYP_METHOD()
    const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    /*! \brief Get the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     *
     *  \return The default font atlas. */
    const RC<FontAtlas>& GetDefaultFontAtlas() const;

    /*! \brief Set the default font atlas to use for text rendering.
     *  UIText objects will use this font atlas if they don't have a font atlas set.
     *
     *  \param fontAtlas The font atlas to set. */
    void SetDefaultFontAtlas(RC<FontAtlas> fontAtlas);

    /*! \brief Get the UI object that is currently focused. If no object is focused, returns nullptr.
     *  \return The focused UI object. */
    HYP_FORCE_INLINE const WeakHandle<UIObject>& GetFocusedObject() const
    {
        return m_focusedObject;
    }

    UIEventHandlerResult OnInputEvent(
        InputManager* inputManager,
        const SystemEvent& event);

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vec2f& position, Array<Handle<UIObject>>& outObjects, EnumFlags<UIRayTestFlags> flags = UIRayTestFlags::DEFAULT);

    virtual void AddChildUIObject(const Handle<UIObject>& uiObject) override;

protected:
    virtual void Init() override;

    virtual void Update_Internal(float delta) override;

    virtual void OnAttached_Internal(UIObject* parent) override;

    // Override OnRemoved_Internal to update subobjects to have this as a stage
    virtual void OnRemoved_Internal() override;

    virtual void SetStage_Internal(UIStage* stage) override;

private:
    virtual void ComputeActualSize(const UIObjectSize& inSize, Vec2i& outActualSize, UpdateSizePhase phase, bool isInner) override;

    /*! \brief To be called internally from UIObject only */
    void SetFocusedObject(const Handle<UIObject>& uiObject);

    Handle<UIObject> GetUIObjectForEntity(const Entity* entity) const;

    bool Remove(const Entity* entity);

    Vec2i m_surfaceSize;

    Handle<Scene> m_scene;
    Handle<Camera> m_camera;

    RC<FontAtlas> m_defaultFontAtlas;

    HashMap<WeakHandle<UIObject>, UIObjectPressedState> m_mouseButtonPressedStates;
    FlatSet<WeakHandle<UIObject>> m_hoveredUiObjects;
    HashMap<KeyCode, Array<WeakHandle<UIObject>>> m_keyedDownObjects;

    WeakHandle<UIObject> m_focusedObject;

    DelegateHandler m_onCurrentWindowChangedHandler;
};

} // namespace hyperion

#endif