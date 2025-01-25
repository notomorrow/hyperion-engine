/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_SUBSYSTEM_HPP
#define HYPERION_EDITOR_SUBSYSTEM_HPP

#include <editor/ui/EditorUI.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorTask.hpp>

#include <scene/Subsystem.hpp>
#include <scene/NodeProxy.hpp>

#include <core/containers/Array.hpp>

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
class FontAtlas;
class EditorDelegates;
class EditorSubsystem;
class EditorProject;
class EditorDebugOverlayBase;
class AssetPackage;

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

HYP_CLASS()
class HYP_API EditorSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(EditorSubsystem);

public:
    EditorSubsystem(const RC<AppContext> &app_context, const RC<UIStage> &ui_stage);
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
    HYP_FORCE_INLINE const RC<UIStage> &GetUIStage() const
        { return m_ui_stage; }

    HYP_METHOD()
    HYP_FORCE_INLINE EditorActionStack *GetActionStack() const
        { return m_action_stack.Get(); }

    HYP_METHOD()
    HYP_FORCE_INLINE const RC<EditorProject> &GetCurrentProject() const
        { return m_current_project; }

    HYP_METHOD()
    void NewProject();

    HYP_METHOD()
    void OpenProject(const RC<EditorProject> &project);

    HYP_METHOD()
    void AddTask(const RC<IEditorTask> &task);

    HYP_METHOD()
    void SetFocusedNode(const NodeProxy &focused_node);

    HYP_METHOD()
    void AddDebugOverlay(const RC<EditorDebugOverlayBase> &debug_overlay);

    HYP_METHOD()
    bool RemoveDebugOverlay(Name name);

    HYP_METHOD()
    HYP_FORCE_INLINE const NodeProxy &GetFocusedNode() const
        { return m_focused_node; }

    HYP_FORCE_INLINE EditorDelegates *GetEditorDelegates()
        { return m_editor_delegates; }

    Delegate<void, const NodeProxy &, const NodeProxy &>    OnFocusedNodeChanged;

    Delegate<void, EditorProject *>                         OnProjectClosing;
    Delegate<void, EditorProject *>                         OnProjectOpened;

private:
    void LoadEditorUIDefinitions();
    
    void CreateHighlightNode();

    void InitViewport();
    void InitSceneOutline();
    void InitContentBrowser();
    void InitDetailView();
    void InitDebugOverlays();
    
    RC<FontAtlas> CreateFontAtlas();

    void UpdateCamera(GameCounter::TickUnit delta);
    void UpdateTasks(GameCounter::TickUnit delta);
    void UpdateDebugOverlays(GameCounter::TickUnit delta);

    void AddPackageToContentBrowser(const Handle<AssetPackage> &package, bool nested);
    void SetSelectedPackage(const Handle<AssetPackage> &package);

    RC<AppContext>                                                      m_app_context;
    Handle<Scene>                                                       m_scene;
    Handle<Camera>                                                      m_camera;
    RC<UIStage>                                                         m_ui_stage;

    OwningRC<EditorActionStack>                                         m_action_stack;

    RC<EditorProject>                                                   m_current_project;

    FixedArray<Array<RunningEditorTask>, ThreadType::THREAD_TYPE_MAX>   m_tasks_by_thread_type;

    Handle<Texture>                                                     m_scene_texture;
    RC<UIObject>                                                        m_main_panel;

    NodeProxy                                                           m_focused_node;
    // the actual node that displays the highlight for the focused item
    NodeProxy                                                           m_highlight_node;

    bool                                                                m_editor_camera_enabled;
    bool                                                                m_should_cancel_next_click;

    EditorDelegates                                                     *m_editor_delegates;

    Array<RC<EditorDebugOverlayBase>>                                   m_debug_overlays;
    RC<UIObject>                                                        m_debug_overlay_ui_object;

    RC<UIListView>                                                      m_content_browser_directory_list;
    Handle<AssetPackage>                                                m_selected_package;

    DelegateHandlerSet                                                  m_delegate_handlers;
};

} // namespace hyperion

#endif
