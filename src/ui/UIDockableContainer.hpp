/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_DOCKABLE_CONTAINER_HPP
#define HYPERION_UI_DOCKABLE_CONTAINER_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

enum class UIDockableContainerFlow : uint32
{
    HORIZONTAL = 0,
    VERTICAL
};

enum class UIDockableItemPosition : uint32
{
    UNDOCKED = 0,
    LEFT,
    RIGHT,
    CENTER,
    TOP,
    BOTTOM,

    MAX
};

#pragma region UIDockableItem

class HYP_API UIDockableItem : public UIPanel
{
public:
    UIDockableItem(UIStage *stage, NodeProxy node_proxy);
    UIDockableItem(const UIDockableItem &other)                 = delete;
    UIDockableItem &operator=(const UIDockableItem &other)      = delete;
    UIDockableItem(UIDockableItem &&other) noexcept             = delete;
    UIDockableItem &operator=(UIDockableItem &&other) noexcept  = delete;
    virtual ~UIDockableItem() override                          = default;
};

#pragma endregion UIDockableItem

#pragma region UIDockableContainer

class HYP_API UIDockableContainer : public UIPanel
{
public:
    UIDockableContainer(UIStage *stage, NodeProxy node_proxy);
    UIDockableContainer(const UIDockableContainer &other)                   = delete;
    UIDockableContainer &operator=(const UIDockableContainer &other)        = delete;
    UIDockableContainer(UIDockableContainer &&other) noexcept               = delete;
    UIDockableContainer &operator=(UIDockableContainer &&other) noexcept    = delete;
    virtual ~UIDockableContainer() override                                 = default;

    void AddChildUIObject(UIObject *ui_object, UIDockableItemPosition position);

    virtual void AddChildUIObject(UIObject *ui_object) override;
    virtual bool RemoveChildUIObject(UIObject *ui_object) override;

    virtual void Init() override;
    virtual void UpdateSize(bool update_children) override;

private:
    void UpdateLayout();

    FixedArray<RC<UIDockableItem>, uint32(UIDockableItemPosition::MAX)> m_dockable_items;
};

#pragma endregion UIDockableContainer

} // namespace hyperion

#endif