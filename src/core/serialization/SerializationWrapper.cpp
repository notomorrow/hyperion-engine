#include <core/serialization/SerializationWrapper.hpp>

#include <scene/Node.hpp>

namespace hyperion {

void SerializationWrapper<Node>::OnPostLoad(const Type& value)
{
    // Sets scene to be detached scene, owned by the thread this was invoked on
    value->SetScene(nullptr);
}

} // namespace hyperion