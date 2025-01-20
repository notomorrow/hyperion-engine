/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT const char *UIEventHandlerResult_GetMessage(UIEventHandlerResult *result)
{
    AssertThrow(result != nullptr);

    if (Optional<UTF8StringView> message = result->GetMessage()) {
        return message->Data();
    }

    return nullptr;
}

HYP_EXPORT const char *UIEventHandlerResult_GetFunctionName(UIEventHandlerResult *result)
{
    AssertThrow(result != nullptr);

    if (Optional<ANSIStringView> function_name = result->GetFunctionName()) {
        return function_name->Data();
    }

    return nullptr;
}

HYP_EXPORT void UIObject_Spawn(UIObject *spawn_parent, const HypClass *hyp_class, Name *name, Vec2i *position, UIObjectSize *size, HypData *out_hyp_data)
{
    AssertThrow(spawn_parent != nullptr);
    AssertThrow(hyp_class != nullptr);
    AssertThrow(name != nullptr);
    AssertThrow(position != nullptr);
    AssertThrow(size != nullptr);
    AssertThrow(out_hyp_data != nullptr);

    RC<UIObject> ui_object = spawn_parent->CreateUIObject(hyp_class, *name, *position, *size);
    *out_hyp_data = HypData(std::move(ui_object));
}

} // extern "C"