/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_INPUT_HANDLER_HPP
#define HYPERION_INPUT_HANDLER_HPP

#include <input/Keyboard.hpp>
#include <input/Mouse.hpp>

#include <math/Vector2.hpp>

namespace hyperion {

class InputHandler
{
public:
    InputHandler()          = default;
    virtual ~InputHandler() = default;

    virtual bool OnKeyDown(const KeyboardEvent &event) = 0;
    virtual bool OnKeyUp(const KeyboardEvent &event) = 0;
    virtual bool OnMouseDown(const MouseEvent &event) = 0;
    virtual bool OnMouseUp(const MouseEvent &event) = 0;
    virtual bool OnMouseMove(const MouseEvent &event) = 0;
    virtual bool OnMouseDrag(const MouseEvent &event) = 0;
};

class NullInputHandler : public InputHandler
{
public:
    NullInputHandler()          = default;
    virtual ~NullInputHandler() = default;

    virtual bool OnKeyDown(const KeyboardEvent &event) override { return false; }
    virtual bool OnKeyUp(const KeyboardEvent &event) override   { return false; }
    virtual bool OnMouseDown(const MouseEvent &event) override  { return false; }
    virtual bool OnMouseUp(const MouseEvent &event) override    { return false; }
    virtual bool OnMouseMove(const MouseEvent &event) override  { return false; }
    virtual bool OnMouseDrag(const MouseEvent &event) override  { return false; }
};

} // namespace hyperion

#endif