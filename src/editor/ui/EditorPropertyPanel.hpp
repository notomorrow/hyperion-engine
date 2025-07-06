/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <ui/UIPanel.hpp>

namespace hyperion {

HYP_CLASS(Abstract)
class HYP_API EditorPropertyPanelBase : public UIPanel
{
    HYP_OBJECT_BODY(EditorPropertyPanelBase);

public:
    EditorPropertyPanelBase();
    EditorPropertyPanelBase(const EditorPropertyPanelBase& other) = delete;
    EditorPropertyPanelBase& operator=(const EditorPropertyPanelBase& other) = delete;
    EditorPropertyPanelBase(EditorPropertyPanelBase&& other) noexcept = delete;
    EditorPropertyPanelBase& operator=(EditorPropertyPanelBase&& other) noexcept = delete;
    virtual ~EditorPropertyPanelBase() override;

    HYP_METHOD(Scriptable)
    void Build(const HypData& hypData);

protected:
    virtual void Init() override;

    virtual void Build_Impl(const HypData& hypData)
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void UpdateSize_Internal(bool updateChildren) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

    Handle<UIPanel> m_panel;
};

} // namespace hyperion

