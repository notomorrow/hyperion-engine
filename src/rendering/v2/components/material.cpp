#include "material.h"

#include "rendering/v2/engine.h"

namespace hyperion::v2 {

Material::Material()
    : EngineComponentBase(),
      m_shader_data_state(ShaderDataState::DIRTY)
{
}

Material::~Material()
{
    Teardown();
}

void Material::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_MATERIALS, [this](Engine *engine) {
        UpdateShaderData(engine);
    }));
}

void Material::UpdateShaderData(Engine *engine) const
{
    MaterialShaderData shader_data{
        .albedo          = GetParameter<Vector4>(MATERIAL_KEY_ALBEDO),
        .metalness       = GetParameter<float>(MATERIAL_KEY_METALNESS),
        .roughness       = GetParameter<float>(MATERIAL_KEY_ROUGHNESS),
        .subsurface      = GetParameter<float>(MATERIAL_KEY_SUBSURFACE),
        .specular        = GetParameter<float>(MATERIAL_KEY_SPECULAR),
        .specular_tint   = GetParameter<float>(MATERIAL_KEY_SPECULAR_TINT),
        .anisotropic     = GetParameter<float>(MATERIAL_KEY_ANISOTROPIC),
        .sheen           = GetParameter<float>(MATERIAL_KEY_SHEEN),
        .sheen_tint      = GetParameter<float>(MATERIAL_KEY_SHEEN_TINT),
        .clearcoat       = GetParameter<float>(MATERIAL_KEY_CLEARCOAT),
        .clearcoat_gloss = GetParameter<float>(MATERIAL_KEY_CLEARCOAT_GLOSS),
        .emissiveness    = GetParameter<float>(MATERIAL_KEY_EMISSIVENESS),
        .uv_scale        = GetParameter<float>(MATERIAL_KEY_UV_SCALE),
        .parallax_height = GetParameter<float>(MATERIAL_KEY_PARALLAX_HEIGHT)
    };

    const size_t num_bound_textures = MathUtil::Min(
        m_textures.Size(),
        MaterialShaderData::max_bound_textures
    );
    
    for (size_t i = 0; i < num_bound_textures; i++) {
        if (const auto &texture_id = m_textures.ValueAt(i)) {
            if (engine->shader_globals->textures.GetResourceIndex(texture_id, &shader_data.texture_index[i].index)) {
                shader_data.texture_index[i].used = 1;

                continue;
            }

            DebugLog(LogType::Warn, "Texture %d could not be bound for Material %d because it is not found in the bindless texture store\n", texture_id.value, m_id.value);
        }

        shader_data.texture_index[i].used = 0;
    }

    engine->shader_globals->materials.Set(m_id.value - 1, std::move(shader_data));

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    m_parameters.Set(key, value);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::SetTexture(TextureKey key, Texture::ID id)
{
    m_textures.Set(key, id);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

} // namespace hyperion::v2