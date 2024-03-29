#include <core/lib/Delegate.hpp>

namespace hyperion {

namespace functional {
namespace detail {
    
DelegateHandlerData::~DelegateHandlerData()
{
    if (IsValid()) {
        remove_fn(delegate, id);
    }
}

void DelegateHandlerData::Release()
{
    id = 0;
    delegate = nullptr;
    remove_fn = nullptr;
}

} // namespace detail
} // namespace functional

DelegateHandler::DelegateHandler(RC<functional::detail::DelegateHandlerData> data)
    : m_data(std::move(data))
{
}

} // namespace hyperion