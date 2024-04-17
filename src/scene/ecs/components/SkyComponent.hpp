/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SKY_COMPONENT_HPP
#define HYPERION_ECS_SKY_COMPONENT_HPP

#include <core/Handle.hpp>

namespace hyperion {

class SkydomeRenderer;

struct SkyComponent
{
    RC<SkydomeRenderer> render_component;
};

} // namespace hyperion

#endif