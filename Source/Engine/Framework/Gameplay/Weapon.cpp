/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#include <Framework/Gameplay/Weapon.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Weapon.generated.inl>

namespace Hyperion {

Weapon::~Weapon()
{
    if (m_mesh.IsValid())
    {
        EnqueueDeletion(std::move(m_mesh));
    }

    if (m_material.IsValid())
    {
        EnqueueDeletion(std::move(m_material));
    }
}

} // namespace Hyperion
