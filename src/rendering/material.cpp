#include "material.h"

#include <engine.h>

namespace hyperion::v2 {

Material::Material(const char *tag)
    : EngineComponentBase(),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    size_t len = std::strlen(tag);
    m_tag = new char[len + 1];
    std::strcpy(m_tag, tag);

    ResetParameters();
}

Material::~Material()
{
    Teardown();
}

void Material::Init(Engine *engine)
{
    if (IsInitCalled()) {
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

        EnqueueRenderUpdates(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_MATERIALS, [this](Engine *engine) {
            m_textures.Clear();
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }), engine);
    }));
}

void Material::Update(Engine *engine)
{
    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    EnqueueRenderUpdates(engine);
}

void Material::EnqueueRenderUpdates(Engine *engine)
{
    engine->render_scheduler.Enqueue([this, engine](...) {
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

        shader_data.texture_usage = 0;

        const size_t num_bound_textures = MathUtil::Min(
            m_textures.Size(),
            MaterialShaderData::max_bound_textures
        );

        if (num_bound_textures != 0) {
            for (size_t i = 0; i < num_bound_textures; i++) {
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
                        shader_data.texture_usage |= 1 << i;

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
        }

        engine->shader_globals->materials.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Material::SetParameter(MaterialKey key, const Parameter &value)
{
    if (m_parameters[key] == value) {
        return;
    }

    m_parameters.Set(key, value);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Material::ResetParameters()
{
    m_parameters.Set(MATERIAL_KEY_ALBEDO,          Vector4(1.0f));
    m_parameters.Set(MATERIAL_KEY_METALNESS,       0.0f);
    m_parameters.Set(MATERIAL_KEY_ROUGHNESS,       0.65f);
    m_parameters.Set(MATERIAL_KEY_SUBSURFACE,      0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR,        0.0f);
    m_parameters.Set(MATERIAL_KEY_SPECULAR_TINT,   0.0f);
    m_parameters.Set(MATERIAL_KEY_ANISOTROPIC,     0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN,           0.0f);
    m_parameters.Set(MATERIAL_KEY_SHEEN_TINT,      0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT,       0.0f);
    m_parameters.Set(MATERIAL_KEY_CLEARCOAT_GLOSS, 0.0f);
    m_parameters.Set(MATERIAL_KEY_EMISSIVENESS,    0.0f);
    m_parameters.Set(MATERIAL_KEY_UV_SCALE,        Vector2(1.0f));
    m_parameters.Set(MATERIAL_KEY_PARALLAX_HEIGHT, 0.08f);
}


void Material::SetTexture(TextureKey key, Ref<Texture> &&texture)
{
    if (m_textures[key] == texture) {
        return;
    }

    if (texture && IsInitCalled()) {
        texture.Init();
    }

    m_textures.Set(key, std::move(texture));

    m_shader_data_state |= ShaderDataState::DIRTY;
}

Texture *Material::GetTexture(TextureKey key) const
{
    return m_textures.Get(key).ptr;
}

MaterialGroup::MaterialGroup() = default;
MaterialGroup::~MaterialGroup() = default;

} // namespace hyperion::v2