#include "material.h"

#include "rendering/v2/engine.h"

namespace hyperion::v2 {

Material::Material(const char *tag)
    : EngineComponentBase(),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    size_t len = std::strlen(tag);
    m_tag = new char[len + 1];
    std::strcpy(m_tag, tag);
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
        for (size_t i = 0; i < m_textures.Size(); i++) {
            if (auto &texture = m_textures.ValueAt(i)) {
                if (texture == nullptr) {
                    continue;
                }

                texture.Init();
            }
        }

        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_MATERIALS, [this](Engine *engine) {
            m_textures.Clear();
        }), engine);
    }));
}

void Material::Update(Engine *engine)
{
    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    UpdateShaderData(engine);
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
        shader_data.texture_usage[i] = 0;

        if (const auto &texture = m_textures.ValueAt(i)) {
            if (texture == nullptr) {
                DebugLog(
                    LogType::Warn,
                    "Texture could not be bound for Material %d because it is null\n",
                    m_id.value
                );

                continue;
            }

            if (engine->shader_globals->textures.GetResourceIndex(texture->GetId(), &shader_data.texture_index[i])) {
                shader_data.texture_usage[i] = 1;

                continue;
            }

            DebugLog(
                LogType::Warn,
                "Texture #%d could not be bound for Material %d because it is not found in the bindless texture store\n",
                texture->GetId().value,
                m_id.value
            );
        }
    }

    engine->shader_globals->materials.Set(m_id.value - 1, std::move(shader_data));

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    m_parameters.Set(key, value);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::SetTexture(TextureKey key, Ref<Texture> &&texture)
{
    if (texture && IsInit()) {
        texture.Init();
    }

    m_textures.Set(key, std::move(texture));

    m_shader_data_state |= ShaderDataState::DIRTY;
}

Texture *Material::GetTexture(TextureKey key) const
{
    return m_textures.Get(key).ptr;
}

MaterialLibrary::MaterialLibrary() = default;
MaterialLibrary::~MaterialLibrary() = default;

} // namespace hyperion::v2