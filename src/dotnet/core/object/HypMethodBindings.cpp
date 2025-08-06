/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/Name.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Object.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void HypMethod_GetName(const HypMethod* method, Name* outName)
    {
        if (!method || !outName)
        {
            return;
        }

        *outName = method->GetName();
    }

    HYP_EXPORT void HypMethod_GetReturnTypeId(const HypMethod* method, TypeId* outReturnTypeId)
    {
        if (!method || !outReturnTypeId)
        {
            return;
        }

        *outReturnTypeId = method->GetTypeId();
    }

    HYP_EXPORT uint32 HypMethod_GetParameters(const HypMethod* method, const HypMethodParameter** outParams)
    {
        if (!method || !outParams)
        {
            return 0;
        }

        if (method->GetParameters().Empty())
        {
            return 0;
        }

        *outParams = method->GetParameters().Begin();

        return (uint32)method->GetParameters().Size();
    }

    HYP_EXPORT uint32 HypMethod_GetFlags(const HypMethod* method)
    {
        if (!method)
        {
            return uint32(HypMethodFlags::NONE);
        }

        return uint32(method->GetFlags());
    }

    HYP_EXPORT bool HypMethod_Invoke(const HypMethod* method, HypData* args, uint32 numArgs, HypData* outResult)
    {
        if (!method || !outResult)
        {
            return false;
        }

        if (numArgs != 0 && !args)
        {
            return false;
        }

        Span<HypData> argsView(args, numArgs);

        *outResult = method->Invoke(argsView);

        return true;
    }

} // extern "C"