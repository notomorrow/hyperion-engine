/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/utilities/Optional.hpp>

#include <core/containers/String.hpp>

#include <core/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class Node;
class HypProperty;

struct EditorNodePropertyRef
{
    String title;
    Optional<String> description;
    WeakHandle<Node> node;
    HypProperty* property = nullptr;
};

class UIElementFactoryBase;

HYP_API UIElementFactoryBase* GetEditorUIElementFactory(TypeId typeId);

template <class T>
static UIElementFactoryBase* GetEditorUIElementFactory()
{
    return GetEditorUIElementFactory(TypeId::ForType<T>());
}

} // namespace hyperion
