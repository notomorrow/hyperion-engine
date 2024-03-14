#ifndef HYPERION_V2_ECS_ENTITY_TAG_HPP
#define HYPERION_V2_ECS_ENTITY_TAG_HPP

#include <Types.hpp>

namespace hyperion::v2 {

enum class EntityTag : uint32
{
    NONE,
    STATIC,
    LIGHT,

    MAX
};

/*! \brief An EntityTag is a special component that is used to tag an entity with a specific flag.
 * 
 *  \tparam Tag The flag value
 */
template <EntityTag tag>
struct EntityTagComponent
{
    static constexpr EntityTag value = tag;
};

} // namespace hyperion::v2

#endif