/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Input/Mouse.hpp>

#include <Editor/EditorCamera.hpp>

namespace Hyperion {

class Camera;
class View;
class Scene;
class EditorSubsystem;
class ApplicationWindow;
struct WindowOptions;

/*! \brief Turns raw window events of a viewport that isn't the main window into the mouse events the UIStage would produce. */
struct EditorViewportInputState
{
    Vec2f mousePosition;
    Vec2f mouseDownPosition;
    EnumFlags<MouseButtonState> heldButtons = MouseButtonState::NONE;
    bool hovered = false;
    bool dragged = false;
};

HYP_CLASS()
class EditorViewport : public ObjectBase
{
    HYP_OBJECT_BODY(EditorViewport);

public:
    explicit EditorViewport(const Handle<Camera>& camera = Handle<Camera>::Null());
    ~EditorViewport() override;

    HYP_METHOD()
    HYP_FORCE_INLINE EditorViewportType GetViewportType() const
    {
        return m_viewportType;
    }

    /*! \brief Must be called before the viewport is initialized. */
    HYP_METHOD()
    void SetViewportType(EditorViewportType viewportType);

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsOrthographic() const
    {
        return m_viewportType != EditorViewportType::Perspective;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    /*! \brief Sim thread. */
    HYP_METHOD()
    ApplicationWindow* GetWindow() const;

    /*! \brief Main thread. The main viewport's window becomes the app's main window, the others are presented alongside it. */
    HYP_METHOD()
    Handle<ApplicationWindow> CreateViewportWindow(const WindowOptions& options, bool isMainWindow);

    /*! \brief Main thread. Stops presenting the window before its native handle goes away. */
    HYP_METHOD()
    void ReleaseViewportWindow();

    /*! \brief Ortho viewports only redraw when something changed; this forces the next frame to. */
    HYP_METHOD()
    void RequestRedraw();

    HYP_FORCE_INLINE EditorViewportInputState& GetInputState()
    {
        return m_inputState;
    }

    void Init() override;

    /*! \brief Called when the viewport is added to the editor subsystem,
     *  or when a new project is opened. */
    void OnAdded(EditorSubsystem* editorSubsystem);

    /*! \brief Ortho viewports start centered on the perspective camera, once there is one. */
    void TryCenterOnMainViewport(EditorSubsystem* editorSubsystem);

    /*! \brief Called when the viewport is removed from the editor subsystem
     *  or when the current project is closed. */
    void OnRemoved(EditorSubsystem* editorSubsystem);

    /*! \brief Called from the editor subsystem when a scene is added to the current project's World. */
    void OnSceneAdded(Scene* scene);

    /*! \brief Called from the editor subsystem when a scene is removed from the current project's World. */
    void OnSceneRemoved(Scene* scene);

    /*! \brief Show only \p scenes (plus the editor scene) in place of the world's scenes. An empty array shows the world's scenes again. */
    void SetIsolatedScenes(EditorSubsystem* editorSubsystem, const Array<Handle<Scene>>& scenes);

protected:
    bool ShowsWorldScene(const Scene* scene) const;

    void SetWindow(const Handle<ApplicationWindow>& window, bool isMainWindow);
    void UpdateCameraDimensions();

    EditorViewportType m_viewportType;

    Handle<Camera> m_camera;
    Handle<View> m_view;

    Handle<ApplicationWindow> m_window;
    bool m_isMainWindow;
    bool m_hasBeenCentered = false;
    DelegateHandler m_windowSizeChangedHandler;

    EditorViewportInputState m_inputState;

    Array<WeakHandle<Scene>> m_isolatedScenes;
};

} // namespace Hyperion
