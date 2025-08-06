/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <util/GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

class Game;

class GameThread final : public Thread<Scheduler>
{
public:
    GameThread(const Handle<AppContextBase>& appContext);

    void SetGame(const Handle<Game>& game);

private:
    virtual void operator()() override;

    Handle<AppContextBase> m_appContext;
    Handle<Game> m_game;
};

} // namespace hyperion
