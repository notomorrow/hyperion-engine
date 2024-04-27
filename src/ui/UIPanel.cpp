/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <ui/UIPanel.hpp>

#include <Engine.hpp>

namespace hyperion {

UIPanel::UIPanel(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy), UOT_PANEL)
{
    SetBorderRadius(5);
    SetBorderFlags(UI_OBJECT_BORDER_ALL);
}

Handle<Material> UIPanel::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_PANEL" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .layer              = GetDrawableLayer()
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.075f, 0.09f, 0.135f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture> { } }
        }
    );
}

} // namespace hyperion
