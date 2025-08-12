/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorPropertyPanel.hpp>

#include <core/object/HypObject.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API TransformEditorPropertyPanel : public EditorPropertyPanelBase
{
    HYP_OBJECT_BODY(TransformEditorPropertyPanel);

public:
    TransformEditorPropertyPanel();
    virtual ~TransformEditorPropertyPanel() override;

    virtual void Build_Impl(const HypData& hypData, const HypProperty* property) override;
};

} // namespace hyperion

