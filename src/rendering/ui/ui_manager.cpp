#include "ui_manager.h"

namespace hyperion {
UIManager::UIManager(InputManager *input_manager)
    : m_input_manager(input_manager),
      m_input_event(new InputEvent),
      m_mouse_pressed(false)
{
    m_input_event->SetHandler([&](SystemWindow *window, bool pressed) {
        m_mouse_pressed = pressed;
        // process click event on all ui objects
        HandleMouseEvent();
    });
    m_input_manager->RegisterClickEvent(MouseButton::MOUSE_BUTTON_LEFT, *m_input_event);
}

UIManager::~UIManager()
{
    delete m_input_event;
}

void UIManager::Update(double dt)
{
    HandleMouseEvent();
}

void UIManager::HandleMouseEvent()
{
    int mouse_x, mouse_y;
    m_input_manager->GetMousePosition(&mouse_x, &mouse_y);
    Vector2 mouse_vector((float)mouse_x, (float)mouse_y);
    mouse_vector *= 0.5;
    mouse_vector += 0.5;

    // mouse_vector *= 

    for (auto &object : m_ui_objects) {
        // std::cout << "update " << object->GetName() << "\n";
        if (object->IsMouseOver((float)mouse_x, (float)mouse_y)) {
            std::cout << "mouseover " << object->GetName() << "\n";
            object->GetClickEvent().Trigger(nullptr, m_mouse_pressed);
        }
    }
}
} // namespace hyperion
