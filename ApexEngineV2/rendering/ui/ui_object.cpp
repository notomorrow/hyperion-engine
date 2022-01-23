#include "ui_object.h"
#include "../shader_manager.h"
#include "../shaders/ui/ui_object_shader.h"
#include "../../util/mesh_factory.h"

#include <algorithm>

namespace hyperion {
namespace ui {
UIObject::UIObject(const std::string &name)
    : Entity(name)
{
    m_material.depth_test = false;
    m_material.depth_write = false;
    m_material.alpha_blended = true;

    m_renderable = MeshFactory::CreateQuad();
    m_renderable->SetShader(ShaderManager::GetInstance()->GetShader<UIObjectShader>(ShaderProperties()));
    m_renderable->SetRenderBucket(Renderable::RB_SCREEN);
}

void UIObject::UpdateTransform()
{
    Entity::UpdateTransform();
}

bool UIObject::IsMouseOver(double x, double y) const
{
    if (x < m_global_transform.GetTranslation().x) {
        return false;
    }

    if (x > m_global_transform.GetTranslation().x + m_global_transform.GetScale().x) {
        return false;
    }

    if (y < m_global_transform.GetTranslation().y) {
        return false;
    }

    if (y > m_global_transform.GetTranslation().y + m_global_transform.GetScale().y) {
        return false;
    }

    return true;
}
} // namespace ui
} // namespace hyperion
