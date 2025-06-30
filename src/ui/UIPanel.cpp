/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIPanel.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>

#include <core/containers/Bitset.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIPanel::UIPanel()
    : m_isScrollEnabled(UIObjectScrollbarOrientation::ALL)
{
    SetBorderRadius(0);
    SetBackgroundColor(Color(0.025f, 0.025f, 0.025f, 1.0f));
    SetTextColor(Color(0xFFFFFFFFu));

    m_onScrollHandler = OnScroll.Bind([this](const MouseEvent& eventData) -> UIEventHandlerResult
        {
            return HandleScroll(eventData);
        });
}

void UIPanel::OnAttached_Internal(UIObject* parent)
{
    HYP_SCOPE;

    UIObject::OnAttached_Internal(parent);
}

void UIPanel::SetIsScrollEnabled(UIObjectScrollbarOrientation orientation, bool isScrollEnabled)
{
    HYP_SCOPE;

    if (Bitset bitset { uint64(uint8(orientation)) }; bitset.Count() != 1)
    {
        for (Bitset::BitIndex bitIndex : bitset)
        {
            SetIsScrollEnabled(UIObjectScrollbarOrientation(Bitset::GetBitMask(bitIndex)), isScrollEnabled);
        }

        return;
    }

    m_isScrollEnabled[orientation] = isScrollEnabled;

    SetScrollbarVisible(orientation, isScrollEnabled);
}

void UIPanel::SetScrollbarVisible(UIObjectScrollbarOrientation orientation, bool visible)
{
    HYP_SCOPE;

    if (Bitset bitset { uint64(uint8(orientation)) }; bitset.Count() != 1)
    {
        for (Bitset::BitIndex bitIndex : bitset)
        {
            SetScrollbarVisible(UIObjectScrollbarOrientation(Bitset::GetBitMask(bitIndex)), visible);
        }

        return;
    }

    Handle<UIObject>* scrollbar = nullptr;
    UIObjectSize scrollbarSize;
    UIObjectAlignment scrollbarAlignment = UIObjectAlignment::TOP_LEFT;

    switch (orientation)
    {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = &m_verticalScrollbar;
        scrollbarSize = UIObjectSize({ GetVerticalScrollbarSize(), UIObjectSize::PIXEL }, { 100, UIObjectSize::PERCENT });
        scrollbarAlignment = UIObjectAlignment::TOP_RIGHT;

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = &m_horizontalScrollbar;
        scrollbarSize = UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetHorizontalScrollbarSize(), UIObjectSize::PIXEL });
        scrollbarAlignment = UIObjectAlignment::BOTTOM_LEFT;

        break;
    default:
        return;
    }

    if (visible == (*scrollbar != nullptr && (*scrollbar)->IsVisible()))
    {
        // If scrollbar UIObject already exists and we are disabling scroll then return early
        return;
    }

    if (visible)
    {
        // If scrollbar UIObject already exists and we are enabling scroll then return early
        if (!*scrollbar)
        {
            Handle<UIPanel> newScrollbar = CreateUIObject<UIPanel>(NAME("Scrollbar_Panel"), Vec2i { 0, 0 }, scrollbarSize);
            newScrollbar->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
            newScrollbar->SetAffectsParentSize(false);
            newScrollbar->SetIsPositionAbsolute(true);
            newScrollbar->SetIsScrollEnabled(UIObjectScrollbarOrientation::ALL, false);
            newScrollbar->SetAcceptsFocus(false);
            newScrollbar->SetParentAlignment(scrollbarAlignment);
            newScrollbar->SetOriginAlignment(scrollbarAlignment);

            *scrollbar = std::move(newScrollbar);
        }

        UIObject::AddChildUIObject(*scrollbar);

        UpdateScrollbarSize(orientation);
        UpdateScrollbarThumbPosition(orientation);

        SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);
    }
    else
    {
        if (*scrollbar)
        {
            UIObject::RemoveChildUIObject(scrollbar->Get());
            scrollbar->Reset();
        }

        SetDeferredUpdate(UIObjectUpdateType::UPDATE_SIZE, true);
    }
}

void UIPanel::Init()
{
    HYP_SCOPE;

    UIObject::Init();
}

void UIPanel::UpdateSize_Internal(bool updateChildren)
{
    HYP_SCOPE;

    UIObject::UpdateSize_Internal(updateChildren);

    UpdateScrollbarSizes();
}

void UIPanel::OnScrollOffsetUpdate_Internal(Vec2f delta)
{
    HYP_SCOPE;

    // if (!MathUtil::ApproxEqual(delta.x, 0.0f)) {
    //     UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
    // }

    // if (!MathUtil::ApproxEqual(delta.y, 0.0f)) {
    //     UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
    // }

    const Vec2i scrollOffset = GetScrollOffset();

    if (scrollOffset.x != 0)
    {
        UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
    }

    if (scrollOffset.y != 0)
    {
        UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
    }
}

MaterialAttributes UIPanel::GetMaterialAttributes() const
{
    return UIObject::GetMaterialAttributes();
}

Material::ParameterTable UIPanel::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

Material::TextureSet UIPanel::GetMaterialTextures() const
{
    return UIObject::GetMaterialTextures();
}

UIEventHandlerResult UIPanel::HandleScroll(const MouseEvent& eventData)
{
    HYP_SCOPE;

    if ((eventData.wheel.x != 0 && CanScroll(UIObjectScrollbarOrientation::HORIZONTAL)) || (eventData.wheel.y != 0 && CanScroll(UIObjectScrollbarOrientation::VERTICAL)))
    {
        SetScrollOffset(GetScrollOffset() - eventData.wheel * 10, /* smooth */ true);

        return UIEventHandlerResult::STOP_BUBBLING;
    }

    // allow parent to scroll
    return UIEventHandlerResult::OK;
}

void UIPanel::UpdateScrollbarSizes()
{
    HYP_SCOPE;

    const bool isWidthGreater = GetActualInnerSize().x > GetActualSize().x;
    const bool isHeightGreater = GetActualInnerSize().y > GetActualSize().y;

    if (m_isScrollEnabled & UIObjectScrollbarOrientation::HORIZONTAL)
    {
        if (isWidthGreater)
        {
            if (m_horizontalScrollbar == nullptr)
            {
                SetScrollbarVisible(UIObjectScrollbarOrientation::HORIZONTAL, true);
            }
            else
            {
                UpdateScrollbarSize(UIObjectScrollbarOrientation::HORIZONTAL);
                UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::HORIZONTAL);
            }
        }
        else if (m_horizontalScrollbar != nullptr && m_horizontalScrollbar->IsVisible())
        {
            SetScrollbarVisible(UIObjectScrollbarOrientation::HORIZONTAL, false);
        }
    }

    if (m_isScrollEnabled & UIObjectScrollbarOrientation::VERTICAL)
    {
        if (isHeightGreater)
        {
            if (m_verticalScrollbar == nullptr)
            {
                SetScrollbarVisible(UIObjectScrollbarOrientation::VERTICAL, true);
            }
            else
            {
                UpdateScrollbarSize(UIObjectScrollbarOrientation::VERTICAL);
                UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation::VERTICAL);
            }
        }
        else if (m_verticalScrollbar != nullptr && m_verticalScrollbar->IsVisible())
        {
            SetScrollbarVisible(UIObjectScrollbarOrientation::VERTICAL, false);
        }
    }
}

void UIPanel::UpdateScrollbarSize(UIObjectScrollbarOrientation orientation)
{
    HYP_SCOPE;

    const Vec2i visibleContentSize = GetActualSize();
    const Vec2i innerContentSize = GetActualInnerSize();
    const Vec2f ratios = Vec2f(visibleContentSize) / Vec2f(innerContentSize);

    Handle<UIObject> scrollbar;
    UIObjectSize thumbSize;

    switch (orientation)
    {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_verticalScrollbar;
        thumbSize = UIObjectSize({ 100, UIObjectSize::PERCENT }, { int(ratios.y * 100.0f), UIObjectSize::PERCENT });

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontalScrollbar;
        thumbSize = UIObjectSize({ int(ratios.x * 100.0f), UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT });

        break;
    default:
        HYP_UNREACHABLE();
    }

    AssertThrow(scrollbar != nullptr);

    Handle<UIObject> thumb = scrollbar->FindChildUIObject("ScrollbarThumb", /* deep */ false);

    if (thumb)
    {
        thumb->SetSize(thumbSize);
    }
    else
    {
        thumb = CreateUIObject<UIButton>(NAME("ScrollbarThumb"), Vec2i { 0, 0 }, thumbSize);
        thumb->SetBorderRadius(8);
        thumb->SetBackgroundColor(Color(0.1f, 0.15f, 0.22f, 0.75f));
        thumb->SetPadding(0);

        thumb->OnMouseDown.Bind([this, orientation, thumbWeak = thumb.ToWeak()](const MouseEvent& eventData) -> UIEventHandlerResult
                              {
                                  if (Handle<UIObject> thumb = thumbWeak.Lock())
                                  {
                                      const int orientationIndex = UIObjectScrollbarOrientationToIndex(orientation);
                                      AssertThrow(orientationIndex != -1);

                                      m_initialDragPosition[orientationIndex] = Vec2i(eventData.position * Vec2f(thumb->GetActualSize()));
                                  }

                                  return UIEventHandlerResult::STOP_BUBBLING;
                              })
            .Detach();

        thumb->OnMouseUp.Bind([this, orientation](const MouseEvent& eventData) -> UIEventHandlerResult
                            {
                                const int orientationIndex = UIObjectScrollbarOrientationToIndex(orientation);
                                AssertThrow(orientationIndex != -1);

                                m_initialDragPosition[orientationIndex] = Vec2i::Zero();

                                return UIEventHandlerResult::STOP_BUBBLING;
                            })
            .Detach();

        thumb->OnMouseDrag.Bind([this, orientation, scrollbarWeak = scrollbar.ToWeak(), thumbWeak = thumb.ToWeak()](const MouseEvent& eventData) -> UIEventHandlerResult
                              {
                                  if (CanScroll(orientation))
                                  {
                                      Handle<UIObject> thumb = thumbWeak.Lock();
                                      Handle<UIObject> scrollbar = scrollbarWeak.Lock();

                                      if (thumb != nullptr && scrollbar != nullptr)
                                      {
                                          const int orientationIndex = UIObjectScrollbarOrientationToIndex(orientation);
                                          AssertThrow(orientationIndex != -1);

                                          const Vec2f mousePositionRelative = Vec2f(eventData.absolutePosition - m_initialDragPosition[orientationIndex]) - scrollbar->GetAbsolutePosition();
                                          const float mouseRelevantPosition = mousePositionRelative[orientationIndex];

                                          Vec2f ratios;
                                          ratios[orientationIndex] = mouseRelevantPosition / float(scrollbar->GetActualSize()[orientationIndex]);

                                          SetScrollOffset(Vec2i(Vec2f(GetActualInnerSize()) * ratios), /* smooth */ false);
                                      }
                                  }

                                  return UIEventHandlerResult::STOP_BUBBLING;
                              })
            .Detach();

        scrollbar->AddChildUIObject(thumb);
    }
}

void UIPanel::UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation orientation)
{
    HYP_SCOPE;

    Handle<UIObject> scrollbar;

    switch (orientation)
    {
    case UIObjectScrollbarOrientation::VERTICAL:
        scrollbar = m_verticalScrollbar;

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        scrollbar = m_horizontalScrollbar;

        break;
    default:
        HYP_UNREACHABLE();
    }

    if (!m_isScrollEnabled[orientation] || !scrollbar)
    {
        return;
    }

    Handle<UIObject> thumb = scrollbar->FindChildUIObject("ScrollbarThumb", /* deep */ false);

    if (!thumb)
    {
        return;
    }

    const Vec2i visibleContentSizeOffset = GetScrollOffset();
    const Vec2i innerContentSize = GetActualInnerSize() - GetActualSize();
    const Vec2f ratios = Vec2f(visibleContentSizeOffset) / Vec2f(innerContentSize);

    Vec2i position;

    switch (orientation)
    {
    case UIObjectScrollbarOrientation::VERTICAL:
        position = Vec2i { 0, int(ratios.y * float(scrollbar->GetActualSize().y - thumb->GetActualSize().y)) };

        break;
    case UIObjectScrollbarOrientation::HORIZONTAL:
        position = Vec2i { int(ratios.x * float(scrollbar->GetActualSize().x - thumb->GetActualSize().x)), 0 };

        break;
    default:
        HYP_UNREACHABLE();
    }

    thumb->SetPosition(position);
}

} // namespace hyperion
