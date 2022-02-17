#include "ui_object.h"
#include "../shader_manager.h"
#include "../shaders/ui/ui_object_shader.h"
#include "../../util/mesh_factory.h"

#include <algorithm>

namespace hyperion {
namespace ui {
UIObject::UIObject(const std::string &name)
    : Node(name)
{
    GetMaterial().depth_test = false;
    GetMaterial().depth_write = false;
    GetMaterial().alpha_blended = true;

    SetRenderable(MeshFactory::CreateQuad());
    GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<UIObjectShader>(ShaderProperties()));
    GetSpatial().SetBucket(Spatial::Bucket::RB_SCREEN);
}

void UIObject::UpdateTransform()
{
    Node::UpdateTransform();
}

bool UIObject::IsMouseOver(double x, double y) const
{
    if (x < GetGlobalTranslation().x) {
        return false;
    }

    if (x > GetGlobalTranslation().x + GetGlobalScale().x) {
        return false;
    }

    if (y < GetGlobalTranslation().y) {
        return false;
    }

    if (y > GetGlobalTranslation().y + GetGlobalScale().y) {
        return false;
    }

    return true;
}
} // namespace ui
} // namespace hyperion
