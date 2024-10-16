/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_DOTNET_TYPES_HPP
#define HYP_DOTNET_TYPES_HPP

#include <type_traits>

namespace hyperion {
namespace dotnet {

using Delegate = std::add_pointer_t<void()>;

} // namespace dotnet
} // namespace hyperion

#endif