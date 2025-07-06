#pragma once
#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <core/utilities/Variant.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/math/Vector2.hpp>

#include <SDL2/SDL.h>

#include <Types.hpp>

namespace hyperion {
namespace sys {

enum SystemEventType
{
    EVENT_WINDOW_EVENT = SDL_WINDOWEVENT,
    EVENT_KEYDOWN = SDL_KEYDOWN,
    EVENT_SHUTDOWN = SDL_QUIT,
    EVENT_KEYUP = SDL_KEYUP,

    EVENT_MOUSEMOTION = SDL_MOUSEMOTION,
    EVENT_MOUSEBUTTON_DOWN = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSEBUTTON_UP = SDL_MOUSEBUTTONUP,
    EVENT_MOUSESCROLL = SDL_MOUSEWHEEL,

    EVENT_FILE_DROP = SDL_DROPFILE
};

enum SystemWindowEventType
{
    EVENT_WINDOW_MOVED = SDL_WINDOWEVENT_MOVED,
    EVENT_WINDOW_RESIZED = SDL_WINDOWEVENT_RESIZED,

    EVENT_WINDOW_FOCUS_GAINED = SDL_WINDOWEVENT_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST = SDL_WINDOWEVENT_FOCUS_LOST,

    EVENT_WINDOW_CLOSE = SDL_WINDOWEVENT_CLOSE,
    EVENT_WINDOW_MINIMIZED = SDL_WINDOWEVENT_MINIMIZED,
};

class HYP_API SystemEvent
{
public:
    using EventData = Variant<FilePath, void*>;

    SystemEvent() = default;
    SystemEvent(const SystemEvent& other) = delete;
    SystemEvent& operator=(const SystemEvent& other) = delete;

    SystemEvent(SystemEvent&& other) noexcept
        : m_sdlEvent(other.m_sdlEvent),
          m_eventData(std::move(other.m_eventData))
    {
        Memory::MemSet(&other.m_sdlEvent, 0x00, sizeof(SDL_Event));
    }

    SystemEvent& operator=(SystemEvent&& other) noexcept
    {
        // sdlEvent better not have any leakable memory!
        // we free filedrop memory as soon as we receive the event...

        m_sdlEvent = other.m_sdlEvent;
        Memory::MemSet(&other.m_sdlEvent, 0x0, sizeof(SDL_Event));

        m_eventData = std::move(other.m_eventData);

        return *this;
    }

    ~SystemEvent() = default;

    SystemEventType GetType() const
    {
        return (SystemEventType)m_sdlEvent.type;
    }

    SystemWindowEventType GetWindowEventType() const
    {
        return (SystemWindowEventType)m_sdlEvent.window.event;
    }

    KeyCode GetKeyCode() const;

    /*! \brief For any characters a-z, returns the uppercase version.
     *  Otherwise, the result from GetKeyCode() is returned.
     */
    KeyCode GetNormalizedKeyCode() const
    {
        KeyCode key = GetKeyCode();

        // /* Set all letters to uppercase */
        // if (uint32(key) >= 'a' && uint32(key) <= 'z') {
        //     key = KeyCode('A' + (uint32(key) - 'a'));
        // }

        return key;
    }

    EnumFlags<MouseButtonState> GetMouseButtons() const;

    void GetMouseWheel(int* x, int* y) const
    {
        *x = m_sdlEvent.wheel.x;
        *y = m_sdlEvent.wheel.y;
    }

    uint32 GetWindowId() const
    {
        return m_sdlEvent.window.windowID;
    }

    Vec2i GetWindowResizeDimensions()
    {
        SDL_Event* ev = this->GetInternalEvent();

        Vec2i result;
        result.x = ev->window.data1;
        result.y = ev->window.data2;

        return result;
    }

    SDL_Event* GetInternalEvent();

    EventData& GetEventData()
    {
        return m_eventData;
    }

    const EventData& GetEventData() const
    {
        return m_eventData;
    }

private:
    SDL_Event m_sdlEvent;

    EventData m_eventData;
};

} // namespace sys

using sys::SystemEvent;
using sys::SystemEventType;
using sys::SystemWindowEventType;

} // namespace hyperion
