/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIImage.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIGrid.hpp>

#include <dotnet/core/RefCountedPtrBindings.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void UIStage_GetScene(UIStage *stage, ManagedHandle *out_scene)
{
    *out_scene = CreateManagedHandleFromHandle(stage->GetScene());
}

HYP_EXPORT void UIStage_GetSurfaceSize(UIStage *stage, Vec2i *out_surface_size)
{
    *out_surface_size = stage->GetSurfaceSize();
}

#define DEF_CREATE_UI_OBJECT(T) \
    HYP_EXPORT void UIStage_CreateUIObject_##T(UIStage *stage, Name *name, Vec2i *position, Vec2i *size, bool attach_to_root, ManagedRefCountedPtr *out_ptr) \
    { \
        RC<T> ui_object = stage->CreateUIObject<T>(*name, *position, UIObjectSize({ size->x, UIObjectSize::PIXEL }, { size->y, UIObjectSize::PIXEL }), attach_to_root); \
        *out_ptr = CreateManagedRefCountedPtr(ui_object); \
    }

DEF_CREATE_UI_OBJECT(UIButton)
DEF_CREATE_UI_OBJECT(UIText)
DEF_CREATE_UI_OBJECT(UIPanel)
DEF_CREATE_UI_OBJECT(UIImage)
DEF_CREATE_UI_OBJECT(UITabView)
DEF_CREATE_UI_OBJECT(UIGrid)

#undef DEF_CREATE_UI_OBJECT

} // extern "C"