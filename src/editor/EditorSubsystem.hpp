/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_SUBSYSTEM_HPP
#define HYPERION_EDITOR_SUBSYSTEM_HPP

#include <editor/ui/EditorUI.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorTask.hpp>

#include <scene/Subsystem.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/UUID.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class World;
class Scene;
class Camera;
class Texture;
class InputManager;
class UIStage;
class UIObject;
class UIListView;
class UIGrid;
class FontAtlas;
class EditorDelegates;
class EditorSubsystem;
class EditorProject;
class EditorDebugOverlayBase;
class AssetPackage;
struct MouseEvent;
class View;
class ScreenCaptureRenderSubsystem;
class ConsoleUI;

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

class RunningEditorTask
{
public:
    friend class EditorSubsystem;

    RunningEditorTask(const RC<IEditorTask> &task)
        : m_task(task)
    {
    }

    RunningEditorTask(const RC<IEditorTask> &task, const RC<UIObject> &ui_object)
        : m_task(task),
          m_ui_object(ui_object)
    {
    }

    HYP_FORCE_INLINE const RC<IEditorTask> &GetTask() const
        { return m_task; }

    HYP_FORCE_INLINE const RC<UIObject> &GetUIObject() const
        { return m_ui_object; }

    RC<UIObject> CreateUIObject(UIStage *ui_stage) const;

private:
    RC<IEditorTask> m_task;
    RC<UIObject>    m_ui_object;
};

HYP_CLASS()
class GenerateLightmapsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateLightmapsEditorTask);

public:
    GenerateLightmapsEditorTask(const Handle<World> &world, const Handle<Scene> &scene);
    virtual void Process() override;

protected:
    virtual void Cancel_Impl() override;
    virtual bool IsCompleted_Impl() const override;
    virtual void Tick_Impl(float delta) override;

private:
    Handle<World>   m_world;
    Handle<Scene>   m_scene;
    Task<void>      *m_task;
};

enum class EditorManipulationMode
{
    NONE        = 0,

    TRANSLATE,
    ROTATE,
    SCALE
};

/*! \brief A widget that can manipulate the selected object. (e.g translate, rotate, scale) */
HYP_CLASS(Abstract)
class HYP_API EditorManipulationWidgetBase : public EnableRefCountedPtrFromThis<EditorManipulationWidgetBase>
{
    HYP_OBJECT_BODY(EditorManipulationWidgetBase);

public:
    EditorManipulationWidgetBase();
    virtual ~EditorManipulationWidgetBase() = default;

    HYP_FORCE_INLINE const Handle<Node> &GetNode() const
        { return m_node; }

    HYP_FORCE_INLINE const Handle<Node> &GetFocusedNode() const
        { return m_focused_node; }

    HYP_FORCE_INLINE bool IsDragging() const
        { return m_is_dragging; }

    void Initialize();
    void Shutdown();

    virtual EditorManipulationMode GetManipulationMode() const = 0;

    virtual int GetPriority() const
        { return -1; }
    
    virtual void UpdateWidget(const Handle<Node> &focused_node);

    virtual void OnDragStart(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node, const Vec3f &hitpoint);
    virtual void OnDragEnd(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node);

    virtual bool OnMouseHover(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node)
        { return false; }

    virtual bool OnMouseLeave(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node)
        { return false; }

    virtual bool OnMouseMove(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node)
        { return false; }

protected:
    virtual Handle<Node> Load_Internal() const = 0;

    Handle<Node>   m_focused_node;
    Handle<Node>   m_node;

private:
    bool        m_is_dragging;
};

HYP_CLASS()
class NullEditorManipulationWidget : public EditorManipulationWidgetBase
{
    HYP_OBJECT_BODY(NullEditorManipulationWidget);

public:
    virtual ~NullEditorManipulationWidget() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
        { return EditorManipulationMode::NONE; }

protected:
    virtual Handle<Node> Load_Internal() const override
        { return Handle<Node>::empty; }
};

HYP_CLASS()
class TranslateEditorManipulationWidget : public EditorManipulationWidgetBase
{
    HYP_OBJECT_BODY(TranslateEditorManipulationWidget);

public:
    virtual ~TranslateEditorManipulationWidget() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
        { return EditorManipulationMode::TRANSLATE; }

    virtual int GetPriority() const override
        { return 0; }

    virtual void OnDragStart(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node, const Vec3f &hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node) override;

    virtual bool OnMouseHover(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node) override;
    virtual bool OnMouseLeave(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node) override;
    virtual bool OnMouseMove(const Handle<Camera> &camera, const MouseEvent &mouse_event, const Handle<Node> &node) override;

protected:
    struct DragData
    {
        Vec3f   axis_direction;
        Vec3f   plane_normal;
        Vec3f   plane_point;
        Vec3f   hitpoint_origin;
        Vec3f   node_origin;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData>  m_drag_data;
};

class HYP_API EditorManipulationWidgetHolder
{
    using EditorManipulationWidgetSet = HashSet<
        RC<EditorManipulationWidgetBase>,
        +[](const RC<EditorManipulationWidgetBase> &ptr) -> EditorManipulationMode { return ptr->GetManipulationMode(); }
    >;

public:
    using OnSelectedManipulationWidgetChangeDelegate = Delegate<void, EditorManipulationWidgetBase &, EditorManipulationWidgetBase &>;

    EditorManipulationWidgetHolder();

    HYP_FORCE_INLINE const EditorManipulationWidgetSet &GetManipulationWidgets() const
        { return m_manipulation_widgets; }

    EditorManipulationMode GetSelectedManipulationMode() const;
    void SetSelectedManipulationMode(EditorManipulationMode mode);

    EditorManipulationWidgetBase &GetSelectedManipulationWidget() const;

    void Initialize();
    void Shutdown();

    OnSelectedManipulationWidgetChangeDelegate  OnSelectedManipulationWidgetChange;

private:
    EditorManipulationMode                      m_selected_manipulation_mode;
    EditorManipulationWidgetSet                 m_manipulation_widgets;
};

HYP_CLASS()
class HYP_API EditorSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(EditorSubsystem);

public:
    EditorSubsystem(const RC<AppContext> &app_context);
    virtual ~EditorSubsystem() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void Update(GameCounter::TickUnit delta) override;

    virtual void OnSceneAttached(const Handle<Scene> &scene) override;
    virtual void OnSceneDetached(const Handle<Scene> &scene) override;

    HYP_FORCE_INLINE const RC<AppContext> &GetAppContext() const
        { return m_app_context; }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene> &GetScene() const
        { return m_scene; }

    HYP_METHOD()
    HYP_FORCE_INLINE EditorActionStack *GetActionStack() const
        { return m_action_stack.Get(); }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<EditorProject> &GetCurrentProject() const
        { return m_current_project; }

    HYP_METHOD()
    void NewProject();

    HYP_METHOD()
    void OpenProject(const Handle<EditorProject> &project);

    HYP_METHOD()
    void AddTask(const RC<IEditorTask> &task);

    HYP_METHOD()
    void SetFocusedNode(const Handle<Node> &focused_node);

    HYP_METHOD()
    void AddDebugOverlay(const RC<EditorDebugOverlayBase> &debug_overlay);

    HYP_METHOD()
    bool RemoveDebugOverlay(WeakName name);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Node> &GetFocusedNode() const
        { return m_focused_node; }

    HYP_FORCE_INLINE EditorDelegates *GetEditorDelegates()
        { return m_editor_delegates; }

    Delegate<void, const Handle<Node> &, const Handle<Node> &>    OnFocusedNodeChanged;

    Delegate<void, const Handle<EditorProject> &>           OnProjectClosing;
    Delegate<void, const Handle<EditorProject> &>           OnProjectOpened;

private:
    void LoadEditorUIDefinitions();
    
    void CreateHighlightNode();

    void InitViewport();
    void InitSceneOutline();
    void InitContentBrowser();
    void InitDetailView();
    void InitConsoleUI();
    void InitDebugOverlays();
    void InitManipulationWidgetSelection();
    
    RC<FontAtlas> CreateFontAtlas();

    void UpdateCamera(GameCounter::TickUnit delta);
    void UpdateTasks(GameCounter::TickUnit delta);
    void UpdateDebugOverlays(GameCounter::TickUnit delta);

    void AddPackageToContentBrowser(const Handle<AssetPackage> &package, bool nested);
    void SetSelectedPackage(const Handle<AssetPackage> &package);

    void SetHoveredManipulationWidget(
        const MouseEvent &event,
        EditorManipulationWidgetBase *manipulation_widget,
        const Handle<Node> &manipulation_widget_node
    );

    HYP_FORCE_INLINE bool IsHoveringManipulationWidget() const
    {
        return m_hovered_manipulation_widget.IsValid() && m_hovered_manipulation_widget_node.IsValid();
    }

    RC<AppContext>                                                      m_app_context;
    Handle<Scene>                                                       m_scene;
    Handle<Camera>                                                      m_camera;

    OwningRC<EditorActionStack>                                         m_action_stack;

    Handle<EditorProject>                                               m_current_project;

    FixedArray<Array<RunningEditorTask>, ThreadType::THREAD_TYPE_MAX>   m_tasks_by_thread_type;

    EditorManipulationWidgetHolder                                      m_manipulation_widget_holder;
    Weak<EditorManipulationWidgetBase>                                  m_hovered_manipulation_widget;
    WeakHandle<Node>                                                          m_hovered_manipulation_widget_node;

    Handle<Texture>                                                     m_scene_texture;
    RC<UIObject>                                                        m_main_panel;

    Handle<Node>                                                           m_focused_node;
    // the actual node that displays the highlight for the focused item
    Handle<Node>                                                           m_highlight_node;

    bool                                                                m_editor_camera_enabled;
    bool                                                                m_should_cancel_next_click;

    EditorDelegates                                                     *m_editor_delegates;

    Array<RC<EditorDebugOverlayBase>>                                   m_debug_overlays;
    RC<UIObject>                                                        m_debug_overlay_ui_object;

    RC<ConsoleUI>                                                       m_console_ui;

    RC<UIListView>                                                      m_content_browser_directory_list;
    Handle<AssetPackage>                                                m_selected_package;

    RC<UIGrid>                                                          m_content_browser_contents;
    RC<UIObject>                                                        m_content_browser_contents_empty;

    Array<Handle<View>>                                                 m_views;
    Array<RC<ScreenCaptureRenderSubsystem>>                             m_screen_capture_render_subsystems;

    DelegateHandlerSet                                                  m_delegate_handlers;
};

} // namespace hyperion

#endif
