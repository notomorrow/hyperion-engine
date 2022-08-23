#include "Controller.hpp"
#include <scene/Entity.hpp>

namespace hyperion::v2 {

std::atomic<ControllerID> ControllerSet::controller_id_counter{0};

Controller::Controller(const String &name, bool receives_update)
    : m_name(name),
      m_owner(nullptr),
      m_receives_update(receives_update)
{
}

Controller::~Controller()
{
}

Engine *Controller::GetEngine() const
{
    AssertThrow(m_owner != nullptr);

    return m_owner->GetEngine();
}

} // namespace hyperion::v2