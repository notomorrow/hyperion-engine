/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_CAMERA_COMPONENT_HPP
#define HYPERION_ECS_CAMERA_COMPONENT_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <HashCode.hpp>

namespace hyperion {

class CameraController;

HYP_STRUCT(Component, Label="Camera Component")
struct CameraComponent
{
    HYP_FIELD(Property="CameraController", Serialize=true, Editor=true)
    Handle<Camera>          camera;

    HYP_FIELD(Property="CameraController", Serialize=true, Editor=true)
    RC<CameraController>    camera_controller;
};

} // namespace hyperion

#endif