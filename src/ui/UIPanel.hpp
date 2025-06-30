/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_PANEL_HPP
#define HYPERION_UI_PANEL_HPP

#include <ui/UIObject.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class UIStage;

#pragma region UIPanel

HYP_CLASS()
class HYP_API UIPanel : public UIObject
{
    HYP_OBJECT_BODY(UIPanel);

public:
    UIPanel();
    UIPanel(const UIPanel& other) = delete;
    UIPanel& operator=(const UIPanel& other) = delete;
    UIPanel(UIPanel&& other) noexcept = delete;
    UIPanel& operator=(UIPanel&& other) noexcept = delete;
    virtual ~UIPanel() override = default;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsHorizontalScrollEnabled() const
    {
        return m_isScrollEnabled & UIObjectScrollbarOrientation::HORIZONTAL;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsVerticalScrollEnabled() const
    {
        return m_isScrollEnabled & UIObjectScrollbarOrientation::VERTICAL;
    }

    HYP_METHOD()
    void SetIsScrollEnabled(UIObjectScrollbarOrientation orientation, bool isScrollEnabled);

    virtual bool CanScroll(UIObjectScrollbarOrientation orientation) const override
    {
        const int orientationIndex = UIObjectScrollbarOrientationToIndex(orientation);
        AssertThrow(orientationIndex != -1);

        return (m_isScrollEnabled & orientation)
            && GetActualInnerSize()[orientationIndex] > GetActualSize()[orientationIndex];
    }

protected:
    virtual void Init() override;

    virtual void OnAttached_Internal(UIObject* parent) override;

    virtual void UpdateSize_Internal(bool updateChildren) override;

    virtual void OnScrollOffsetUpdate_Internal(Vec2f delta) override;

    virtual MaterialAttributes GetMaterialAttributes() const override;
    virtual Material::ParameterTable GetMaterialParameters() const override;
    virtual Material::TextureSet GetMaterialTextures() const override;

private:
    void SetScrollbarVisible(UIObjectScrollbarOrientation orientation, bool visible);

    void UpdateScrollbarSizes();
    void UpdateScrollbarSize(UIObjectScrollbarOrientation orientation);
    void UpdateScrollbarThumbPosition(UIObjectScrollbarOrientation orientation);

    UIEventHandlerResult HandleScroll(const MouseEvent& eventData);

    EnumFlags<UIObjectScrollbarOrientation> m_isScrollEnabled;
    DelegateHandler m_onScrollHandler;

    FixedArray<Vec2i, 2> m_initialDragPosition;
};

#pragma endregion UIPanel

} // namespace hyperion

#endif