#ifndef HYPERION_V2_COMPONENTS_BASE_H
#define HYPERION_V2_COMPONENTS_BASE_H

#include <rendering/backend/renderer_instance.h>

#include <memory>
#include <initializer_list>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;

class Engine;

template <class WrappedType>
class BaseComponent {
    template <class ObjectType, class InnerType>
    struct IdWrapper {
        using InnerType_t = InnerType;

        explicit constexpr operator InnerType() const { return value; }
        inline constexpr InnerType GetValue() const { return value; }

        InnerType value;
    };

public:
    using ID = IdWrapper<WrappedType, uint32_t>;

    BaseComponent() = default;
    BaseComponent(std::unique_ptr<WrappedType> &&wrapped) : m_wrapped(std::move(wrapped)) {}
    BaseComponent(const BaseComponent &other) = delete;
    BaseComponent &operator=(const BaseComponent &other) = delete;
    ~BaseComponent()
    {
        AssertThrowMsg(
            m_wrapped == nullptr,
            "Expected wrapped object of type %s to be destroyed before destructor, but it was not nullptr.",
            typeid(WrappedType).name()
        );
    }

    inline constexpr WrappedType *GetWrappedObject() { return m_wrapped.get(); }
    inline constexpr const WrappedType *GetWrappedObject() const { return m_wrapped.get(); }

protected:
    std::unique_ptr<WrappedType> m_wrapped;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPONENTS_BASE_H

