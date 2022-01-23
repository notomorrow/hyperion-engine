#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "ui_object.h"

#include <vector>
#include <memory>

namespace hyperion {
class InputManager;
class InputEvent;
class UIManager {
public:
    UIManager(InputManager *input_manager);
    ~UIManager();

    inline void RegisterUIObject(const std::shared_ptr<ui::UIObject> &ui_object)
        { m_ui_objects.push_back(ui_object); }

    void Update(double dt);

    void HandleMouseEvent();

private:
    std::vector<std::shared_ptr<ui::UIObject>> m_ui_objects;

    bool m_mouse_pressed;

    InputManager *m_input_manager;
    InputEvent *m_input_event;
};
} // namespace hyperion

#endif
