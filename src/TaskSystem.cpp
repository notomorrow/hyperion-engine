#include <TaskSystem.hpp>

namespace hyperion::v2 {

TaskSystem &TaskSystem::GetInstance()
{
    static TaskSystem instance;

    return instance;
}

} // namespace hyperion::v2