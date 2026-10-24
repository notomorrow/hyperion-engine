#pragma once

#include <Framework/Game.hpp>

namespace Hyperion {

HYP_CLASS()
class NativeTestGame final : public Game
{
    HYP_OBJECT_BODY(NativeTestGame);

public:
    NativeTestGame();
    virtual ~NativeTestGame() override;

protected:
    virtual void AfterContentLoaded() override;
};

} // namespace Hyperion
