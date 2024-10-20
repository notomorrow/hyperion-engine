/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_LIST_VIEW_HPP
#define HYPERION_UI_LIST_VIEW_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {

class UIDataSourceElement;

#pragma region UIListViewItem

HYP_CLASS()
class HYP_API UIListViewItem : public UIObject
{
    HYP_OBJECT_BODY(UIListViewItem);

public:
    friend class UIListView;

    UIListViewItem();
    UIListViewItem(const UIListViewItem &other)                 = delete;
    UIListViewItem &operator=(const UIListViewItem &other)      = delete;
    UIListViewItem(UIListViewItem &&other) noexcept             = delete;
    UIListViewItem &operator=(UIListViewItem &&other) noexcept  = delete;
    virtual ~UIListViewItem() override                          = default;

    virtual void Init() override;

    virtual void AddChildUIObject(const RC<UIObject> &ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    HYP_FORCE_INLINE const RC<UIObject> &GetInnerElement() const
        { return m_inner_element; }

    bool HasSubItems() const;

    HYP_FORCE_INLINE bool IsExpanded() const
        { return m_is_expanded; }

    void SetIsExpanded(bool is_expanded);

protected:
    virtual void SetIsSelectedItem(bool is_selected_item);

    virtual void SetFocusState_Internal(EnumFlags<UIObjectFocusState> focus_state) override;

private:
    RC<UIObject>    m_inner_element;
    RC<UIObject>    m_expanded_element;
    bool            m_is_selected_item;
    bool            m_is_expanded;
};

#pragma endregion UIListViewItem

#pragma region UIListView

HYP_CLASS()
class HYP_API UIListView : public UIPanel
{
    HYP_OBJECT_BODY(UIListView);

public:
    friend class UIListViewItem;

    UIListView();
    UIListView(const UIListView &other)                 = delete;
    UIListView &operator=(const UIListView &other)      = delete;
    UIListView(UIListView &&other) noexcept             = delete;
    UIListView &operator=(UIListView &&other) noexcept  = delete;
    virtual ~UIListView() override                      = default;

    HYP_FORCE_INLINE const Array<RC<UIListViewItem>> &GetListViewItems() const
        { return m_list_view_items; }

    virtual void Init() override;

    virtual void AddChildUIObject(const RC<UIObject> &ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    UIListViewItem *FindListViewItem(const UUID &data_source_element_uuid) const;

    Delegate<void, UIListViewItem *>    OnSelectedItemChange;

protected:
    virtual void UpdateSize_Internal(bool update_children) override;

    virtual void SetDataSource_Internal(UIDataSourceBase *data_source) override;

    void UpdateLayout();

private:
    static UIListViewItem *FindListViewItem(const UIObject *parent_object, const UUID &data_source_element_uuid);

    void AddDataSourceElement(UIDataSourceBase *data_source, UIDataSourceElement *element, UIDataSourceElement *parent);

    Array<RC<UIListViewItem>>   m_list_view_items;
    Weak<UIListViewItem>        m_selected_item;
};

#pragma endregion UIListView

} // namespace hyperion

#endif