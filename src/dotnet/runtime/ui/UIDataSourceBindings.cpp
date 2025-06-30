/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDataSource.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void UIDataSourceBase_Push(UIDataSourceBase* dataSource, const UUID* uuid, HypData* dataPtr, const UUID* parentUuid)
    {
        AssertThrow(dataSource != nullptr);
        AssertThrow(uuid != nullptr);
        AssertThrow(dataPtr != nullptr);
        AssertThrow(parentUuid != nullptr);

        dataSource->Push(*uuid, std::move(*dataPtr), *parentUuid);
    }

    HYP_EXPORT void UIDataSource_SetElementTypeIdAndFactory(UIDataSource* dataSource, const TypeId* elementTypeId, UIElementFactoryBase* elementFactory)
    {
        AssertThrow(dataSource != nullptr);
        AssertThrow(elementTypeId != nullptr);
        AssertThrow(elementFactory != nullptr);

        dataSource->SetElementTypeIdAndFactory(*elementTypeId, elementFactory);
    }

} // extern "C"