#pragma once
#include <core/Defines.hpp>

#include <game/Game.hpp>

namespace hyperion {
class EditorSubsystem;
namespace editor {

class HyperionEditorImpl;

HYP_CLASS(NoScriptBindings)
class HYP_API HyperionEditor : public Game
{
    HYP_OBJECT_BODY(HyperionEditor);

public:
    HyperionEditor();
    HyperionEditor(const HyperionEditor& other) = delete;
    HyperionEditor& operator=(const HyperionEditor& other) = delete;
    HyperionEditor(HyperionEditor&& other) noexcept = delete;
    HyperionEditor& operator=(HyperionEditor&& other) noexcept = delete;
    virtual ~HyperionEditor() override;

    virtual void Logic(float delta) override;
    virtual void OnInputEvent(const SystemEvent& event) override;

protected:
    virtual void Init() override;

    HyperionEditorImpl* m_impl;
    Handle<EditorSubsystem> m_editorSubsystem;
};
} // namespace editor

using editor::HyperionEditor;

} // namespace hyperion

