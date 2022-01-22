#include "ui_text.h"
#include "../shader_manager.h"
#include "../shaders/ui/ui_object_shader.h"
#include "../../util/mesh_factory.h"
#include "../../asset/asset_manager.h"
#include "../../util.h"

#include <algorithm>

namespace apex {
namespace ui {
FontMap::FontMap(const std::shared_ptr<Texture2D> &texture, int num_chars_per_row, int num_chars_per_col, int char_offset)
    : m_texture(texture)
{
    ex_assert(texture != nullptr);

    m_char_size = Vector2(
        m_texture->GetWidth() / num_chars_per_row,
        m_texture->GetHeight() / num_chars_per_col
    );

    int x_position = 0;
    int y_position = 0;

    for (int ch = char_offset; ch < 255; ch++) {
        m_char_texture_coords[ch] = Vector2(
            x_position,
            y_position
        );

        if (x_position == num_chars_per_row - 1) {
            x_position = 0;

            if (y_position == num_chars_per_col - 1) {
                break;
            }

            y_position++;
        } else {
            x_position++;
        }
    }
}

UIText::UIText(const std::string &name, const std::string &text)
    : UIObject(name),
      m_text(text)
{
    m_font_map = new FontMap(
        AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/fonts/courier_new.png"),
        16,
        16,
        32
    );

    std::vector<CharMesh> char_meshes = BuildCharMeshes();

    if (!char_meshes.empty()) {
        CharMesh char_mesh = OptimizeCharMeshes(char_meshes);

        SetRenderable(char_mesh.quad);
        m_material.SetTexture("ColorMap", m_font_map->GetTexture());
        m_renderable->SetShader(ShaderManager::GetInstance()->GetShader<UIObjectShader>(ShaderProperties()));
        m_renderable->SetRenderBucket(Renderable::RB_SCREEN);
    }
}

UIText::~UIText()
{
    delete m_font_map;
}

std::vector<UIText::CharMesh> UIText::BuildCharMeshes() const
{
    std::vector<CharMesh> char_meshes;

    Vector3 character_placement;

    for (size_t i = 0; i < m_text.size(); i++) {
        char ch = m_text[i];

        if (ch == '\n') {
            character_placement.x = 0;
            character_placement.y -= 1.5;
            continue;
        }

        CharMesh cm;
        cm.transform.SetTranslation(character_placement);
        cm.quad = MeshFactory::CreateQuad(false);

        const Vector2 offset = m_font_map->GetCharOffset(ch);
        const Vector2 scaling = m_font_map->GetScaling();

        auto vertices = cm.quad->GetVertices();

        for (auto &vert : vertices) {
            vert.SetTexCoord0(offset + (vert.GetTexCoord0() * scaling));
        }

        cm.quad->SetVertices(vertices, cm.quad->GetIndices());

        character_placement.x += 1.5;

        char_meshes.push_back(cm);
    }

    return char_meshes;
}

UIText::CharMesh UIText::OptimizeCharMeshes(std::vector<CharMesh> char_meshes) const
{
    if (char_meshes.empty()) {
        return CharMesh();
    }

    Transform base_transform;
    base_transform.SetTranslation(Vector3(1, -1, 0));
    base_transform.SetRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(180)));

    char_meshes[0].quad = MeshFactory::TransformMesh(
        char_meshes[0].quad,
        base_transform * char_meshes[0].transform
    );

    for (size_t i = 1; i < char_meshes.size(); i++) {
        char_meshes[0].quad = MeshFactory::MergeMeshes(
            char_meshes[0].quad,
            char_meshes[i].quad,
            Transform(),
            base_transform * char_meshes[i].transform
        );
    }

    return char_meshes[0];
}
} // namespace ui
} // namespace apex
