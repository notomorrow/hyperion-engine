/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIMenuBar.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIImage.hpp>

#include <input/InputManager.hpp>
#include <input/Mouse.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIMenuItem

UIMenuItem::UIMenuItem()
    : UIObject(UIObjectType::MENU_ITEM)
{
    SetBorderRadius(0);
    SetPadding({ 5, 2 });
}

void UIMenuItem::Init()
{
    UIObject::Init();

    RC<UIMenuBar> menu_bar = GetClosestSpawnParent<UIMenuBar>();
    AssertThrow(menu_bar != nullptr);

    RC<UIImage> icon_element = CreateUIObject<UIImage>(NAME("MenuItemIcon"), Vec2i { 0, 0 }, UIObjectSize({ 16, UIObjectSize::PIXEL }, { 16, UIObjectSize::PIXEL }));
    icon_element->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    icon_element->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    icon_element->SetIsVisible(false);
    m_icon_element = icon_element;

    UIObject::AddChildUIObject(m_icon_element);

    RC<UIText> text_element = CreateUIObject<UIText>(NAME("MenuItemText"), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
    text_element->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    text_element->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    text_element->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    text_element->SetText(m_text);
    m_text_element = text_element;

    UIObject::AddChildUIObject(m_text_element);

    RC<UIPanel> drop_down_menu = CreateUIObject<UIPanel>(CreateNameFromDynamicString(HYP_FORMAT("{}_DropDownMenu", m_name)), Vec2i { 0, 0 }, UIObjectSize({ 150, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));
    // drop_down_menu->SetAcceptsFocus(false);
    drop_down_menu->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    drop_down_menu->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    drop_down_menu->SetBorderFlags(UIObjectBorderFlags::BOTTOM | UIObjectBorderFlags::LEFT | UIObjectBorderFlags::RIGHT);
    m_drop_down_menu = drop_down_menu;

    // UpdateDropDownMenu();
}

void UIMenuItem::AddChildUIObject(const RC<UIObject> &ui_object)
{
    auto it = m_menu_items.FindIf([ui_object](const RC<UIObject> &item)
    {
        return item.Get() == ui_object;
    });

    if (it != m_menu_items.End()) {
        HYP_LOG(UI, Warning, "UIMenuItem::AddChildUIObject() called with a UIMenuItem that is already in the menu item");

        return;
    }

    m_menu_items.PushBack(ui_object);

    UpdateDropDownMenu();

    HYP_LOG(UI, Info, "Add child UI object with name {} to menu item", ui_object->GetName());
}

bool UIMenuItem::RemoveChildUIObject(UIObject *ui_object)
{
    return m_drop_down_menu->RemoveChildUIObject(ui_object);

    // auto it = m_menu_items.FindIf([ui_object](const RC<UIObject> &item)
    // {
    //     return item.Get() == ui_object;
    // });

    // if (it == m_menu_items.End()) {
    //     return UIObject::RemoveChildUIObject(ui_object);
    // }

    // m_menu_items.Erase(it);

    // UpdateDropDownMenu();

    // return true;
}

void UIMenuItem::SetIconTexture(const Handle<Texture> &texture)
{
    m_icon_element->SetTexture(texture);
    
    if (texture.IsValid()) {
        m_icon_element->SetIsVisible(true);

        m_text_element->SetPosition(Vec2i { m_icon_element->GetActualSize().x + 5, 0 });
    } else {
        m_icon_element->SetIsVisible(false);

        m_text_element->SetPosition(Vec2i { 0, 0 });
    }
}

void UIMenuItem::SetText(const String &text)
{
    UIObject::SetText(text);

    if (m_text_element != nullptr) {
        m_text_element->SetText(m_text);
    }

    UpdateSize();
}

void UIMenuItem::UpdateDropDownMenu()
{
    AssertThrow(m_drop_down_menu != nullptr);

    m_drop_down_menu->RemoveAllChildUIObjects();

    if (m_menu_items.Empty()) {
        return;
    }

    Vec2i offset = { 0, 0 };

    for (const RC<UIObject> &menu_item : m_menu_items) {
        menu_item->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        menu_item->SetPosition(offset);

        m_drop_down_menu->AddChildUIObject(menu_item);

        HYP_LOG(UI, Debug, "Menu item {} size: {}", menu_item->GetName(), menu_item->GetActualSize());
        
        offset += { 0, menu_item->GetActualSize().y };
    }

    m_drop_down_menu->UpdateSize();
}

void UIMenuItem::SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state)
{
    const EnumFlags<UIObjectFocusState> previous_focus_state = GetFocusState();

    UIObject::SetFocusState_Internal(focus_state);

    if ((previous_focus_state & (UIObjectFocusState::HOVER | UIObjectFocusState::TOGGLED | UIObjectFocusState::PRESSED)) != (focus_state & (UIObjectFocusState::HOVER | UIObjectFocusState::TOGGLED | UIObjectFocusState::PRESSED))) {
        if (focus_state & (UIObjectFocusState::TOGGLED | UIObjectFocusState::PRESSED)) {
            SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 1.0f });
        } else if (focus_state & UIObjectFocusState::HOVER) {
            SetBackgroundColor(Vec4f { 0.5f, 0.5f, 0.5f, 1.0f });
        } else {
            SetBackgroundColor(Vec4f::Zero());
        }
    }
}

void UIMenuItem::OnFontAtlasUpdate_Internal()
{
    UpdateDropDownMenu();
}

#pragma endregion UIMenuItem

#pragma region UIMenuBar

UIMenuBar::UIMenuBar()
    : UIPanel(UIObjectType::MENU_BAR),
      m_drop_direction(UIMenuBarDropDirection::DOWN),
      m_selected_menu_item_index(~0u)
{
    SetBorderRadius(0);
    SetPadding({ 5, 2 });
}

void UIMenuBar::Init()
{
    Threads::AssertOnThread(g_game_thread);

    UIPanel::Init();

    m_container = CreateUIObject<UIPanel>(NAME("MenuItemContents"), Vec2i { 0, 0 }, UIObjectSize({ 80, UIObjectSize::PIXEL }, { 250, UIObjectSize::PIXEL }));
    m_container->SetIsVisible(false);
    m_container->SetBorderFlags(UIObjectBorderFlags::NONE);
    m_container->SetBorderRadius(0);
    m_container->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    m_container->SetOriginAlignment(m_drop_direction == UIMenuBarDropDirection::DOWN ? UIObjectAlignment::TOP_LEFT : UIObjectAlignment::BOTTOM_LEFT);
    m_container->SetPadding({ 1, 1 });
    m_container->SetDepth(100);

    // @TODO: OnRemoved_Internal() , remove m_container from stage

    m_container->OnClick.Bind([this](const MouseEvent &data) -> UIEventHandlerResult
    {
        // Hide container on any item clicked
        SetSelectedMenuItemIndex(~0u);
        // Lose focus of the container (otherwise hovering over other menu items will cause the menu strips to reappear)
        Blur();

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    m_container->OnLoseFocus.Bind([this](const MouseEvent &data) -> UIEventHandlerResult
    {
        // First check that any child items aren't focused before hiding the container.
        if (!HasFocus(true)) {
            SetSelectedMenuItemIndex(~0u);
        }

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    GetStage()->AddChildUIObject(m_container);

    // AddChildUIObject(m_container);
}

void UIMenuBar::OnRemoved_Internal()
{
    UIPanel::OnRemoved_Internal();

    if (m_container != nullptr) {
        m_container->RemoveFromParent();
    }
}

void UIMenuBar::SetDropDirection(UIMenuBarDropDirection drop_direction)
{
    m_drop_direction = drop_direction;

    if (m_container != nullptr) {
        if (m_drop_direction == UIMenuBarDropDirection::DOWN) {
            m_container->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
        } else {
            m_container->SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);
        }

        if (m_selected_menu_item_index != ~0u) {
            const RC<UIMenuItem> &selected_menu_item = m_menu_items[m_selected_menu_item_index];
            AssertThrow(selected_menu_item != nullptr);

            const Vec2i drop_down_menu_position = GetDropDownMenuPosition(selected_menu_item.Get());

            m_container->SetPosition(drop_down_menu_position);
        }
    }
}

void UIMenuBar::SetSelectedMenuItemIndex(uint32 index)
{
    Threads::AssertOnThread(g_game_thread);

    if (index == m_selected_menu_item_index) {
        return;
    }

    m_selected_menu_item_index = index;

    m_container->SetIsVisible(false);

    if (const NodeProxy &node = m_container->GetNode()) {
        node->RemoveAllChildren();
    }

    for (SizeType i = 0; i < m_menu_items.Size(); i++) {
        if (i == m_selected_menu_item_index) {
            continue;
        }

        const RC<UIMenuItem> &menu_item = m_menu_items[i];

        if (!menu_item) {
            continue;
        }

        menu_item->SetFocusState(menu_item->GetFocusState() & ~UIObjectFocusState::TOGGLED);
    }

    if (m_selected_menu_item_index >= m_menu_items.Size()) {
        m_selected_menu_item_index = ~0u;

        return;
    }

    const RC<UIMenuItem> &menu_item = m_menu_items[m_selected_menu_item_index];

    if (!menu_item || !menu_item->GetDropDownMenuElement()) {
        return;
    }

    menu_item->SetFocusState(menu_item->GetFocusState() | UIObjectFocusState::TOGGLED);

    m_container->AddChildUIObject(menu_item->GetDropDownMenuElement());
    m_container->SetSize(UIObjectSize({ menu_item->GetDropDownMenuElement()->GetActualSize().x + m_container->GetPadding().x * 2, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));
    m_container->SetPosition(GetDropDownMenuPosition(menu_item.Get()));
    m_container->SetIsVisible(true);
    m_container->Focus();
}

void UIMenuBar::AddChildUIObject(const RC<UIObject> &ui_object)
{
    if (ui_object->GetType() != UIObjectType::MENU_ITEM) {
        HYP_LOG(UI, Warning, "UIMenuBar::AddChildUIObject() called with a UIObject that is not a UIMenuItem");

        return;
    }

    auto it = m_menu_items.FindIf([ui_object](const RC<UIMenuItem> &item)
    {
        return item.Get() == ui_object;
    });

    if (it != m_menu_items.End()) {
        HYP_LOG(UI, Warning, "UIMenuBar::AddChildUIObject() called with a UIMenuItem that is already in the menu bar");

        return;
    }

    UIPanel::AddChildUIObject(ui_object);

    RC<UIObject> ui_object_rc = FindChildUIObject([ui_object](UIObject *child)
    {
        return child == ui_object;
    });

    AssertThrow(ui_object_rc != nullptr);

    RC<UIMenuItem> menu_item = ui_object_rc.Cast<UIMenuItem>();
    AssertThrow(menu_item != nullptr);

    menu_item->SetSize(UIObjectSize({ 300, UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT }));

    const Name name = menu_item->GetName();

    // Mouse hover: set selected menu item index if this menu bar has focus
    menu_item->OnMouseHover.RemoveAll();
    menu_item->OnMouseHover.Bind([this, name](const MouseEvent &data) -> UIEventHandlerResult
    {
        if (m_container->HasFocus(true)) {
            const uint32 menu_item_index = GetMenuItemIndex(name);

            SetSelectedMenuItemIndex(menu_item_index);
        }

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    // Mouse click: toggle selected menu item index
    menu_item->OnClick.RemoveAll();
    menu_item->OnClick.Bind([this, name](const MouseEvent &data) -> UIEventHandlerResult
    {
        HYP_LOG(UI, Debug, "OnClick item with name {}", name);

        if (data.mouse_buttons == MouseButtonState::LEFT) {
            const uint32 menu_item_index = GetMenuItemIndex(name);

            HYP_LOG(UI, Debug, "Menu item index for item with name {}: {}", name, menu_item_index);

            if (GetSelectedMenuItemIndex() == menu_item_index) {
                SetSelectedMenuItemIndex(~0u);

                m_container->Blur();
            } else {
                SetSelectedMenuItemIndex(menu_item_index);
            }

            HYP_LOG(UI, Debug, "Container size {}", m_container->GetActualSize());
        }

        return UIEventHandlerResult::STOP_BUBBLING;
    }).Detach();

    m_menu_items.PushBack(menu_item);

    UpdateMenuItemSizes();
}

bool UIMenuBar::RemoveChildUIObject(UIObject *ui_object)
{
    auto menu_items_it = m_menu_items.FindIf([ui_object](const RC<UIMenuItem> &menu_item)
    {
        return menu_item.Get() == ui_object;
    });

    if (menu_items_it == m_menu_items.End()) {
        return UIPanel::RemoveChildUIObject(ui_object);
    }

    const bool removed = UIPanel::RemoveChildUIObject(ui_object);

    if (!removed) {
        return false;
    }

    const SizeType index = menu_items_it - m_menu_items.Begin();

    m_menu_items.Erase(menu_items_it);

    UpdateMenuItemSizes();

    if (m_selected_menu_item_index == index) {
        SetSelectedMenuItemIndex(m_menu_items.Any() ? m_menu_items.Size() - 1 : ~0u);
    }

    return true;
}

void UIMenuBar::UpdateSize_Internal(bool update_children)
{
    UIPanel::UpdateSize_Internal(update_children);

    UpdateMenuItemSizes();
}

RC<UIMenuItem> UIMenuBar::AddMenuItem(Name name, const String &text)
{
    RC<UIMenuItem> menu_item = CreateUIObject<UIMenuItem>(name, Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));
    menu_item->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    menu_item->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    menu_item->SetText(text);

    AddChildUIObject(menu_item);

    return menu_item;
}

RC<UIMenuItem> UIMenuBar::GetMenuItem(Name name) const
{
    Threads::AssertOnThread(g_game_thread);

    for (const RC<UIMenuItem> &menu_item : m_menu_items) {
        if (menu_item->GetName() == name) {
            return menu_item;
        }
    }

    return nullptr;
}

uint32 UIMenuBar::GetMenuItemIndex(Name name) const
{
    Threads::AssertOnThread(g_game_thread);

    for (SizeType i = 0; i < m_menu_items.Size(); i++) {
        if (m_menu_items[i]->GetName() == name) {
            return i;
        }
    }

    return ~0u;
}

bool UIMenuBar::RemoveMenuItem(Name name)
{
    Threads::AssertOnThread(g_game_thread);

    const auto it = m_menu_items.FindIf([name](const RC<UIMenuItem> &menu_item)
    {
        return menu_item->GetName() == name;
    });

    if (it == m_menu_items.End()) {
        return false;
    }

    return RemoveChildUIObject(it->Get());
}

void UIMenuBar::UpdateMenuItemSizes()
{
    if (m_menu_items.Empty()) {
        return;
    }

    Vec2i offset = { 0, 0 };

    for (SizeType i = 0; i < m_menu_items.Size(); i++) {
        m_menu_items[i]->SetPosition(offset);
        m_menu_items[i]->SetSize(UIObjectSize({ 0, UIObjectSize::AUTO }, { 100, UIObjectSize::PERCENT }));

        offset.x += m_menu_items[i]->GetActualSize().x;
    }
}

Vec2i UIMenuBar::GetDropDownMenuPosition(UIMenuItem *menu_item) const
{
    AssertThrow(menu_item != nullptr);
    Vec2f absolute_position = menu_item->GetAbsolutePosition();

    if (m_drop_direction == UIMenuBarDropDirection::DOWN) {
        return Vec2i { int(absolute_position.x), int(absolute_position.y + menu_item->GetActualSize().y) };
    } else {
        return Vec2i { int(absolute_position.x), int(absolute_position.y) };
    }
}

#pragma region UIMenuBar

} // namespace hyperion
