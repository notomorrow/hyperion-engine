#include "ui_button.h"
#include "../shader_manager.h"
#include "../shaders/ui/ui_button_shader.h"
#include "../../util/mesh_factory.h"

#include <algorithm>

namespace hyperion {
namespace ui {
UIButton::UIButton(const std::string &name)
    : UIObject(name)
{
    GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<UIButtonShader>(ShaderProperties()));
}
} // namespace ui
} // namespace hyperion
