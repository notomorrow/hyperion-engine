//
// Created by ethan on 2/5/22.
//

#include "SdlSystem.hpp"
#include "Debug.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "rendering/backend/vulkan/RendererInstance.hpp"

namespace hyperion {

SDL_Event *SystemEvent::GetInternalEvent() {
    return &(this->sdl_event);
}

ApplicationWindow::ApplicationWindow(const ANSIString &title, UInt width, UInt height)
    : m_title(title),
      m_width(width),
      m_height(height)
{
}

SDLApplicationWindow::SDLApplicationWindow(const ANSIString &title, UInt width, UInt height)
    : ApplicationWindow(title, width, height),
      window(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(this->window);
}

void SDLApplicationWindow::Initialize()
{
    this->window = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Int(m_width), Int(m_height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );

    AssertThrowMsg(this->window != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

#ifdef HYP_VULKAN
VkSurfaceKHR SDLApplicationWindow::CreateVkSurface(renderer::Instance *instance)
{
    VkSurfaceKHR surface;
    SDL_bool result = SDL_Vulkan_CreateSurface(window, instance->GetInstance(), &surface);

    AssertThrowMsg(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

    return surface;
}
#endif

void SDLApplicationWindow::SetMousePosition(Int x, Int y)
{
    SDL_WarpMouseInWindow(window, x, y);
}

MouseState SDLApplicationWindow::GetMouseState()
{
    MouseState mouse_state { };
    mouse_state.mask = SDL_GetMouseState(&mouse_state.x, &mouse_state.y);

    return mouse_state;
}

Extent2D SDLApplicationWindow::GetExtent() const
{
    Int width, height;
    SDL_GetWindowSize(window, &width, &height);

    return Extent2D { UInt(width), UInt(height) };
}

void SDLApplicationWindow::SetMouseLocked(bool locked)
{
    SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
}

SDLApplication::SDLApplication()
    : Application()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

SDLApplication::~SDLApplication()
{
    SDL_Quit();
}

UniquePtr<ApplicationWindow> SDLApplication::CreateSystemWindow(const ANSIString &title, UInt width, UInt height)
{
    UniquePtr<SDLApplicationWindow> window;
    window.Reset(new SDLApplicationWindow(title.Data(), width, height));
    window->Initialize();

    return window;
}

Int SDLApplication::PollEvent(SystemEvent &event)
{
    return SDL_PollEvent(event.GetInternalEvent());
}

#ifdef HYP_VULKAN
bool SDLApplication::GetVkExtensions(Array<const char *> &out_extensions) const
{
    UInt32 num_extensions = 0;
    SDL_Window *window = static_cast<SDLApplicationWindow *>(m_current_window.Get())->GetInternalWindow();

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, nullptr)) {
        return false;
    }
    
    out_extensions.Resize(num_extensions);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, out_extensions.Data())) {
        return false;
    }

    return true;
}
#endif

std::vector<const char *> SDLApplication::GetVulkanExtensionNames()
{
    UInt32 num_extensions = 0;
    SDL_Window *window = static_cast<SDLApplicationWindow *>(m_current_window.Get())->GetInternalWindow();

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, nullptr)) {
        ThrowError();
    }

    std::vector<const char *> extensions(num_extensions);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, extensions.data())) {
        ThrowError();
    }

    return extensions;
}

void SDLApplication::ThrowError()
{
    AssertThrowMsg(false, "SDL Error: %s", SDL_GetError());
}

Application::Application()
{
}

} // namespace hyperion