/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENTITY_HPP
#define HYPERION_ENTITY_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

namespace hyperion {

class EntityManager;

HYP_CLASS()
class Entity final : public HypObject<Entity>
{
    HYP_OBJECT_BODY(Entity);

    HYP_PROPERTY(ID, &Entity::GetID, { { "serialize", false } });

public:
    HYP_API Entity();
    HYP_API virtual ~Entity() override;
};

} // namespace hyperion

#endif
