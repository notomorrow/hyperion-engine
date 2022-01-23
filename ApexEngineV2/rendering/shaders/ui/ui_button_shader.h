#ifndef UI_BUTTON_SHADER_H
#define UI_BUTTON_SHADER_H

#include "ui_object_shader.h"

namespace hyperion {
class UIButtonShader : public UIObjectShader {
public:
    UIButtonShader(const ShaderProperties &properties);
    virtual ~UIButtonShader() = default;
};
} // namespace hyperion

#endif
