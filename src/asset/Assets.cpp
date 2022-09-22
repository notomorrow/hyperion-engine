#include "Assets.hpp"
#include <Engine.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

ComponentSystem &AssetManager::GetObjectSystem()
{
    AssertThrow(m_engine != nullptr);

    return m_engine->GetObjectSystem();
}

} // namespace hyperion::v2