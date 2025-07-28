/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/System.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion {

Name SystemBase::GetName() const
{
    return InstanceClass()->GetName();
}

Scene* SystemBase::GetScene() const
{
    return GetEntityManager().GetScene();
}

World* SystemBase::GetWorld() const
{
    return GetEntityManager().GetWorld();
}

} // namespace hyperion