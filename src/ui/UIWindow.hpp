/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_WINDOW_HPP
#define HYPERION_UI_WINDOW_HPP

#include <ui/UIPanel.hpp>

#include <core/utilities/Optional.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

#pragma region UIWindow

class HYP_API UIWindow : public UIPanel
{
public:
    UIWindow(UIStage *stage, NodeProxy node_proxy);
    UIWindow(const UIWindow &other)                 = delete;
    UIWindow &operator=(const UIWindow &other)      = delete;
    UIWindow(UIWindow &&other) noexcept             = delete;
    UIWindow &operator=(UIWindow &&other) noexcept  = delete;
    virtual ~UIWindow() override                    = default;

    virtual void SetText(const String &text) override;

    virtual void Init() override;

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void UpdateSize(bool update_children = true) override;

protected:
    RC<UIPanel>     m_title_bar;
    RC<UIPanel>     m_content;

private:
    Optional<Vec2i> m_mouse_drag_start;
};

#pragma endregion UIWindow

} // namespace hyperion

#endif