/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const char* UIEventHandlerResult_GetMessage(UIEventHandlerResult* result)
    {
        AssertThrow(result != nullptr);

        if (Optional<ANSIStringView> message = result->GetMessage())
        {
            return message->Data();
        }

        return nullptr;
    }

    HYP_EXPORT const char* UIEventHandlerResult_GetFunctionName(UIEventHandlerResult* result)
    {
        AssertThrow(result != nullptr);

        if (Optional<ANSIStringView> functionName = result->GetFunctionName())
        {
            return functionName->Data();
        }

        return nullptr;
    }

    HYP_EXPORT void UIObject_Spawn(UIObject* spawnParent, const HypClass* hypClass, Name* name, Vec2i* position, UIObjectSize* size, HypData* outHypData)
    {
        AssertThrow(spawnParent != nullptr);
        AssertThrow(hypClass != nullptr);
        AssertThrow(name != nullptr);
        AssertThrow(position != nullptr);
        AssertThrow(size != nullptr);
        AssertThrow(outHypData != nullptr);

        Handle<UIObject> uiObject = spawnParent->CreateUIObject(hypClass, *name, *position, *size);
        *outHypData = HypData(std::move(uiObject));
    }

    HYP_EXPORT int8 UIObject_Find(UIObject* parent, const HypClass* hypClass, Name* name, HypData* outHypData)
    {
        AssertThrow(parent != nullptr);
        AssertThrow(hypClass != nullptr);
        AssertThrow(name != nullptr);
        AssertThrow(outHypData != nullptr);

        if (!hypClass->IsDerivedFrom(UIObject::Class()))
        {
            return false;
        }

        Handle<UIObject> uiObject = parent->FindChildUIObject([hypClass, name](UIObject* uiObject)
            {
                return uiObject->IsA(hypClass) && uiObject->GetName() == *name;
            });

        if (!uiObject)
        {
            return false;
        }

        *outHypData = HypData(std::move(uiObject));

        return true;
    }

} // extern "C"