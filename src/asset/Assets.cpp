#include "Assets.hpp"
#include <Engine.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

Assets::Assets(Engine *engine)
    : m_engine(engine),
      m_base_path(FileSystem::CurrentPath())
{
}

Assets::~Assets()
{
    
}

ComponentSystem &AssetManager::GetObjectSystem()
{
    AssertThrow(m_engine != nullptr);

    return m_engine->GetObjectSystem();
}

} // namespace hyperion::v2