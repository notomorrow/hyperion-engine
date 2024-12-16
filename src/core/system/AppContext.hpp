/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_APP_CONTEXT_HPP
#define HYPERION_APP_CONTEXT_HPP

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/functional/Delegate.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Memory.hpp>

#include <core/config/Config.hpp>

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>
#include <core/Handle.hpp>

#include <input/Mouse.hpp>
#include <input/InputManager.hpp>

#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion {

class Game;

namespace renderer {
namespace platform {

// forward declare Instance
template <PlatformType PLATFORM>
class Instance;

} // namespace platform

using Instance = platform::Instance<Platform::CURRENT>;

} // namespace renderer

enum class WindowFlags : uint32
{
    NONE        = 0x0,
    HEADLESS    = 0x1,
    NO_GFX      = 0x2,
    HIGH_DPI    = 0x4
};

HYP_MAKE_ENUM_FLAGS(WindowFlags)

namespace sys {

class SystemEvent;
class CommandLineArguments;
struct ArgParseDefinitions;

struct WindowOptions
{
    ANSIString  title;
    Vec2i       size;
    WindowFlags flags = WindowFlags::NONE;
};

HYP_CLASS(Abstract)
class HYP_API ApplicationWindow : public EnableRefCountedPtrFromThis<ApplicationWindow>
{
    HYP_OBJECT_BODY(ApplicationWindow);

public:
    ApplicationWindow(ANSIString title, Vec2i size);
    ApplicationWindow(const ApplicationWindow &other)               = delete;
    ApplicationWindow &operator=(const ApplicationWindow &other)    = delete;
    virtual ~ApplicationWindow()                                    = default;

    virtual void SetMousePosition(Vec2i position) = 0;
    virtual Vec2i GetMousePosition() const = 0;

    virtual Vec2i GetDimensions() const = 0;
    virtual void HandleResize(Vec2i new_size);

    virtual void SetIsMouseLocked(bool locked) = 0;
    virtual bool HasMouseFocus() const = 0;

    virtual bool IsHighDPI() const
        { return false; }

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(renderer::Instance *instance) = 0;
#endif

    Delegate<void, Vec2i>   OnWindowSizeChanged;

protected:
    ANSIString              m_title;
    Vec2i                   m_size;
};

HYP_CLASS()
class HYP_API SDLApplicationWindow : public ApplicationWindow
{
    HYP_OBJECT_BODY(SDLApplicationWindow);

public:
    SDLApplicationWindow(ANSIString title, Vec2i size);
    virtual ~SDLApplicationWindow() override;

    virtual void SetMousePosition(Vec2i position) override;
    virtual Vec2i GetMousePosition() const override;

    virtual Vec2i GetDimensions() const override;

    virtual void SetIsMouseLocked(bool locked) override;
    virtual bool HasMouseFocus() const override;

    virtual bool IsHighDPI() const override;
    
    void Initialize(WindowOptions);

    SDL_Window *GetInternalWindow()
        { return window; }

    const SDL_Window *GetInternalWindow() const
        { return window; }

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(renderer::Instance *instance) override;
#endif
    
private:
    SDL_Window  *window = nullptr;
};

HYP_CLASS()
class HYP_API AppContext : public EnableRefCountedPtrFromThis<AppContext>
{
    HYP_OBJECT_BODY(AppContext);

public:
    AppContext(ANSIString name, const CommandLineArguments &arguments);
    virtual ~AppContext();

    HYP_FORCE_INLINE const ANSIString &GetAppName() const
        { return m_name; }

    const CommandLineArguments &GetArguments() const;

    HYP_FORCE_INLINE GlobalConfig &GetConfiguration()
        { return m_configuration; }

    HYP_FORCE_INLINE const GlobalConfig &GetConfiguration() const
        { return m_configuration; }

    HYP_FORCE_INLINE ApplicationWindow *GetMainWindow() const
        { return m_main_window.Get(); }

    void SetMainWindow(const RC<ApplicationWindow> &window);

    HYP_FORCE_INLINE const Handle<InputManager> &GetInputManager() const
        { return m_input_manager; }

    void SetGame(Game *game);
    Game *GetGame() const;

    virtual RC<ApplicationWindow> CreateSystemWindow(WindowOptions) = 0;
    virtual int PollEvent(SystemEvent &event) = 0;

    virtual void UpdateConfigurationOverrides();

    virtual const ArgParseDefinitions &GetArgParseDefinitions() const;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char *> &out_extensions) const = 0;
#endif

    Delegate<void, ApplicationWindow *> OnCurrentWindowChanged;
    
protected:
    RC<ApplicationWindow>           m_main_window;
    Handle<InputManager>            m_input_manager;
    ANSIString                      m_name;
    UniquePtr<CommandLineArguments> m_arguments;
    GlobalConfig                    m_configuration;
    Game                            *m_game;
};

HYP_CLASS()
class HYP_API SDLAppContext : public AppContext
{
    HYP_OBJECT_BODY(SDLAppContext);

public:
    SDLAppContext(ANSIString name, const CommandLineArguments &arguments);
    virtual ~SDLAppContext() override;

    virtual RC<ApplicationWindow> CreateSystemWindow(WindowOptions) override;

    virtual int PollEvent(SystemEvent &event) override;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char *> &out_extensions) const override;
#endif

};

} // namespace sys

using sys::SystemEvent;

using sys::WindowOptions;

using sys::AppContext;
using sys::ApplicationWindow;

using sys::SDLAppContext;
using sys::SDLApplicationWindow;

} // namespace hyperion

#endif
