/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Handle_Get(HypObjectBase* ptr, ValueStorage<HypData>* outHypData)
    {
        AssertThrow(outHypData != nullptr);
        AssertThrow(ptr != nullptr);

        outHypData->Construct(AnyRef(ptr->GetObjectHeader_Internal()->container->GetObjectTypeId(), ptr));
    }

    HYP_EXPORT void Handle_Set(HypData* hypData, HypObjectBase** outPtr)
    {
        AssertThrow(outPtr != nullptr);

        if (hypData != nullptr)
        {
            AnyHandle& handle = hypData->Get<AnyHandle>();

            if (handle.IsValid())
            {
                *outPtr = handle.ptr;

                (void)handle.Release();

                return;
            }
        }

        *outPtr = nullptr;
    }

    HYP_EXPORT void Handle_Destruct(HypObjectBase* ptr)
    {
        if (ptr != nullptr)
        {
            ptr->GetObjectHeader_Internal()->DecRefStrong();
        }
    }

    HYP_EXPORT uint8 WeakHandle_Lock(HypObjectBase* ptr)
    {
        AssertThrow(ptr != nullptr);

        if (ptr->GetObjectHeader_Internal()->GetRefCountStrong() == 0)
        {
            return 0;
        }

        ptr->GetObjectHeader_Internal()->IncRefStrong();

        return 1;
    }

    HYP_EXPORT void WeakHandle_Set(HypData* hypData, HypObjectBase** outPtr)
    {
        AssertThrow(outPtr != nullptr);

        if (hypData != nullptr)
        {
            AnyHandle& handle = hypData->Get<AnyHandle>();

            if (handle.IsValid())
            {
                handle.ptr->GetObjectHeader_Internal()->IncRefWeak();

                *outPtr = handle.ptr;

                handle.Reset();

                return;
            }
        }

        *outPtr = nullptr;
    }

    HYP_EXPORT void WeakHandle_Destruct(HypObjectBase* ptr)
    {
        if (ptr != nullptr)
        {
            ptr->GetObjectHeader_Internal()->DecRefWeak();
        }
    }

} // extern "C"