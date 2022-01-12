#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "ui_object.h"

namespace apex {
namespace ui {
class UIButton : public UIObject {
public:
    UIButton(const std::string &name = "");
    virtual ~UIButton() = default;
};
} // namespace ui
} // namespace apex


#endif
