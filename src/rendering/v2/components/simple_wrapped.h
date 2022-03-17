#ifndef HYPERION_V2_SIMPLE_WRAPPED_H
#define HYPERION_V2_SIMPLE_WRAPPED_H

#include "base.h"

#include <memory>

namespace hyperion::v2 {

Device *GetEngineDevice(Engine *engine);

template <class WrappedType>
class SimpleWrapped : public BaseComponent<WrappedType> {
public:
    SimpleWrapped() : BaseComponent<WrappedType>(), m_is_created(false) {}

    template <class ...Args>
    SimpleWrapped(Args &&... args) : BaseComponent<WrappedType>(std::move(args)...), m_is_created(false) {}

    SimpleWrapped(std::unique_ptr<WrappedType> &&wrapped) : BaseComponent<WrappedType>(std::move(wrapped)), m_is_created(false) {}
    SimpleWrapped(const SimpleWrapped &other) = delete;
    SimpleWrapped &operator=(const SimpleWrapped &other) = delete;
    SimpleWrapped(SimpleWrapped &&other) : BaseComponent<WrappedType>(std::move(other.m_wrapped)), m_is_created(other.m_is_created) {}

    SimpleWrapped &operator=(SimpleWrapped &&other) noexcept
    {
        this->m_wrapped = std::move(other.m_wrapped);
        this->m_is_created = other.m_is_created;

        return *this;
    }

    template <class ...Args>
    void Create(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

        AssertThrow(this->m_wrapped != nullptr);

        AssertThrowMsg(
            !m_is_created,
            "Expected wrapped object of type %s to have not already been created, but it was already created.",
            wrapped_type_name
        );

        auto result = this->m_wrapped->Create(GetEngineDevice(engine), std::move(args)...);
        AssertThrowMsg(result, "Creation of object of type %s failed: %s", wrapped_type_name, result.message);

        m_is_created = true;
    }

    template <class ...Args>
    void Destroy(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

        AssertThrow(this->m_wrapped != nullptr);

        AssertThrowMsg(
            m_is_created,
            "Expected wrapped object of type %s to have been created, but it was not yet created.",
            wrapped_type_name
        );

        auto result = this->m_wrapped->Destroy(GetEngineDevice(engine), std::move(args)...);
        AssertThrowMsg(result, "Destruction of object of type %s failed: %s", wrapped_type_name, result.message);

        this->m_wrapped.reset();

        m_is_created = false;
    }

private:
    bool m_is_created;
};

} // namespace hyperion::v2

#endif
