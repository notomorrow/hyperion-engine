#include <ui/UIText.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <util/MeshBuilder.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {
struct UICharMesh
{
    Handle<Mesh>    quad_mesh;
    Transform       transform;
};

static Array<UICharMesh> BuildCharMeshes(const FontMap &font_map, const String &text)
{
    Array<UICharMesh> char_meshes;
    Vec3f placement;

    const SizeType length = text.Length();

    for (SizeType i = 0; i < length; i++) {
        char ch = text[i];

        if (ch == '\n') {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y -= 1.5f;

            continue;
        }

        UICharMesh char_mesh;
        char_mesh.transform.SetTranslation(placement);
        char_mesh.quad_mesh = MeshBuilder::Quad();

        const Vec2f offset = font_map.GetCharOffset(ch);
        const Vec2f scaling = font_map.GetScaling();

        auto streamed_mesh_data = char_mesh.quad_mesh->GetStreamedMeshData();
        AssertThrow(streamed_mesh_data != nullptr);

        auto ref = streamed_mesh_data->AcquireRef();

        Array<Vertex> vertices = ref->GetMeshData().vertices;
        const Array<uint32> &indices = ref->GetMeshData().indices;

        for (Vertex &vert : vertices) {
            vert.SetTexCoord0(offset + (vert.GetTexCoord0() * scaling));
        }

        Mesh::SetStreamedMeshData(
            char_mesh.quad_mesh,
            StreamedMeshData::FromMeshData({
                std::move(vertices),
                indices
            })
        );

        placement.x += 1.5f;

        char_meshes.PushBack(std::move(char_mesh));
    }

    return char_meshes;
}

static Handle<Mesh> OptimizeCharMeshes(Array<UICharMesh> &&char_meshes)
{
    if (char_meshes.Empty()) {
        return { };
    }

    Transform base_transform;
    base_transform.SetTranslation(Vector3(1.0f, -1.0f, 0.0f));

    Handle<Mesh> transformed_mesh = MeshBuilder::ApplyTransform(char_meshes[0].quad_mesh.Get(), base_transform * char_meshes[0].transform);

    for (SizeType i = 1; i < char_meshes.Size(); i++) {
        transformed_mesh = MeshBuilder::Merge(
            transformed_mesh.Get(),
            char_meshes[i].quad_mesh.Get(),
            Transform(),
            base_transform * char_meshes[i].transform
        );
    }

    InitObject(transformed_mesh);

    return transformed_mesh;
}

// UIText

Handle<Mesh> UIText::BuildTextMesh(const FontMap &font_map, const String &text)
{
    return OptimizeCharMeshes(BuildCharMeshes(font_map, text));
}

UIText::UIText(ID<Entity> entity, UIScene *parent)
    : UIObject(entity, parent),
      m_text("No text set"),
      m_font_map(parent != nullptr ? parent->GetDefaultFontMap() : nullptr)
{
}

void UIText::Init()
{
    UIObject::Init();

    UpdateMesh();
}

void UIText::SetText(const String &text)
{
    m_text = text;

    UpdateMesh();
}

void UIText::SetFontMap(RC<FontMap> font_map)
{
    m_font_map = std::move(font_map);

    UpdateMesh(true);
}

void UIText::UpdateMesh(bool update_material)
{
    MeshComponent &mesh_component = GetParent()->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    mesh_component.mesh = m_font_map != nullptr
        ? BuildTextMesh(*m_font_map, m_text)
        : Handle<Mesh> { };

    if (update_material || !mesh_component.material.IsValid()) {
        mesh_component.material = GetMaterial();
    }

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;

    BoundingBoxComponent &bounding_box_component = GetParent()->GetScene()->GetEntityManager()->GetComponent<BoundingBoxComponent>(GetEntity());
    bounding_box_component.local_aabb = mesh_component.mesh.IsValid() ? mesh_component.mesh->GetAABB() : BoundingBox::empty;
}

Handle<Material> UIText::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 1.0f, 1.0f, 1.0f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, m_font_map != nullptr ? m_font_map->GetTexture() : Handle<Texture> { } }
        }
    );
}

} // namespace hyperion::v2