#ifndef HYPERION_APP_HPP
#define HYPERION_APP_HPP

#include <core/Defines.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class Game;

namespace cli {

class CommandLineArguments;

} // namespace cli

using cli::CommandLineArguments;

namespace sys {

class AppContextBase;

class HYP_API App
{
public:
    App();
    App(const App& other) = delete;
    App& operator=(const App& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App&& other) noexcept = delete;
    virtual ~App();

    const RC<AppContextBase>& GetAppContext() const
    {
        return m_app_context;
    }

    virtual void Launch(Game* game, const CommandLineArguments& arguments) final;

protected:
    virtual RC<AppContextBase> InitAppContext(Game* game, const CommandLineArguments& arguments);

private:
    void RunMainLoop(Game* game);

    RC<AppContextBase> m_app_context;
};

} // namespace sys

using sys::App;

} // namespace hyperion

#endif