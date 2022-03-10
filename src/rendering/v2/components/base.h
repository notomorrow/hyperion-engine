#ifndef HYPERION_V2_COMPONENTS_BASE_H
#define HYPERION_V2_COMPONENTS_BASE_H

#include <rendering/backend/renderer_instance.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;

template <class WrappedType>
class BaseComponent {
public:
    using ID = int;

    BaseComponent() = default;
    BaseComponent(std::unique_ptr<WrappedType> &&wrapped) : m_wrapped(std::move(wrapped)) {}
    BaseComponent(const BaseComponent &other) = delete;
    BaseComponent &operator=(const BaseComponent &other) = delete;
    ~BaseComponent()
    {
        AssertExitMsg(
            m_wrapped == nullptr,
            "Expected wrapped object of type %s to be destroyed before destructor, but it was not nullptr.",
            typeid(WrappedType).name()
        );
    }

    inline WrappedType *GetWrappedObject() { return m_wrapped.get(); }
    inline const WrappedType *GetWrappedObject() const { return m_wrapped.get(); }

    void Create(Instance *instance)  {}
    void Destroy(Instance *instance) {}

protected:
    std::unique_ptr<WrappedType> m_wrapped;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPONENTS_BASE_H

