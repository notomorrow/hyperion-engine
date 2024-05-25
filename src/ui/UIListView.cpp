/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <input/InputManager.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region UIListViewItem

UIListViewItem::UIListViewItem(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::LIST_VIEW_ITEM)
{
}

void UIListViewItem::Init()
{
    UIPanel::Init();
}

#pragma endregion UIListViewItem

#pragma region UIListView

UIListView::UIListView(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::LIST_VIEW)
{
}

void UIListView::Init()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    AssertThrow(GetStage() != nullptr);
}

void UIListView::AddChildUIObject(UIObject *ui_object)
{
    // RC<UIListViewItem> list_view_item = GetStage()->CreateUIObject<UIListViewItem>(Name::Unique("ListViewItem"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    // list_view_item->AddChildUIObject(ui_object);

    // m_list_view_items.PushBack(list_view_item);

    // UIObject::AddChildUIObject(std::move(list_view_item));

    if (!ui_object) {
        return;
    }

    m_list_view_items.PushBack(ui_object);

    UIObject::AddChildUIObject(ui_object);

    UpdateLayout();
}

bool UIListView::RemoveChildUIObject(UIObject *ui_object)
{
    if (!ui_object) {
        return false;
    }
    
    for (uint i = 0; i < m_list_view_items.Size(); i++) {
        if (m_list_view_items[i] == ui_object) {
            UIObject::RemoveChildUIObject(ui_object);
            m_list_view_items.EraseAt(i);

            UpdateLayout();

            return true;
        }
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UIListView::UpdateSize(bool update_children)
{
    UIPanel::UpdateSize(update_children);

    UpdateLayout();
}

void UIListView::UpdateLayout()
{
    if (m_list_view_items.Empty()) {
        return;
    }

    int y_offset = 0;

    const Vec2i actual_size = GetActualSize();

    for (uint i = 0; i < m_list_view_items.Size(); i++) {
        UIObject *list_view_item = m_list_view_items[i];

        if (!list_view_item) {
            continue;
        }

        list_view_item->SetPosition({ 0, y_offset });

        y_offset += list_view_item->GetActualSize().y;
    }
}

#pragma region UIListView

} // namespace hyperion
