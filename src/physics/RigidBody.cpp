#include <physics/RigidBody.hpp>

#include <Engine.hpp>

namespace hyperion::v2::physics {

RigidBody::~RigidBody()
{
    Teardown();
}

void RigidBody::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    // do nothing
}

} // namespace hyperion::v2::physics