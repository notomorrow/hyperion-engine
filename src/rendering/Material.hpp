#ifndef HYPERION_V2_MATERIAL_H
#define HYPERION_V2_MATERIAL_H

#include "Texture.hpp"
#include "Shader.hpp"
#include "RenderableAttributes.hpp"

#include <core/lib/FixedArray.hpp>
#include <core/lib/String.hpp>
#include <Types.hpp>

#include <util/EnumOptions.hpp>
#include <HashCode.hpp>

#include <array>

namespace hyperion {
namespace renderer {

class DescriptorSet;

} // namespace renderer
} // namespace hyperion

namespace hyperion::v2 {

class Material : public EngineComponentBase<STUB_CLASS(Material)>
{
public:
    static constexpr UInt max_parameters = 32u;
    static constexpr UInt max_textures = 32u;

    enum TextureKey : UInt64
    {
        MATERIAL_TEXTURE_NONE = 0,

        MATERIAL_TEXTURE_ALBEDO_MAP    = 1 << 0,
        MATERIAL_TEXTURE_NORMAL_MAP    = 1 << 1,
        MATERIAL_TEXTURE_AO_MAP        = 1 << 2,
        MATERIAL_TEXTURE_PARALLAX_MAP  = 1 << 3,
        MATERIAL_TEXTURE_METALNESS_MAP = 1 << 4,
        MATERIAL_TEXTURE_ROUGHNESS_MAP = 1 << 5,
        MATERIAL_TEXTURE_SKYBOX_MAP    = 1 << 6,
        MATERIAL_TEXTURE_COLOR_MAP     = 1 << 7,
        MATERIAL_TEXTURE_POSITION_MAP  = 1 << 8,
        MATERIAL_TEXTURE_DATA_MAP      = 1 << 9,
        MATERIAL_TEXTURE_SSAO_MAP      = 1 << 10,
        MATERIAL_TEXTURE_TANGENT_MAP   = 1 << 11,
        MATERIAL_TEXTURE_BITANGENT_MAP = 1 << 12,
        MATERIAL_TEXTURE_DEPTH_MAP     = 1 << 13,

        // terrain

        MATERIAL_TEXTURE_SPLAT_MAP = 1 << 14,

        MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP      = 1 << 15,
        MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP     = 1 << 16,
        MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP         = 1 << 17,
        MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP   = 1 << 18,

        MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP    = 1 << 19,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP   = 1 << 20,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP       = 1 << 21,
        MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP = 1 << 22,

        MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP    = 1 << 23,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP   = 1 << 24,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP       = 1 << 25,
        MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP = 1 << 26
    };

    struct Parameter
    {
        union
        {
            float float_values[4];
            int int_values[4];
            void *ptr;
        } values;

        enum Type : UInt
        {
            MATERIAL_PARAMETER_TYPE_NONE,
            MATERIAL_PARAMETER_TYPE_FLOAT,
            MATERIAL_PARAMETER_TYPE_FLOAT2,
            MATERIAL_PARAMETER_TYPE_FLOAT3,
            MATERIAL_PARAMETER_TYPE_FLOAT4,
            MATERIAL_PARAMETER_TYPE_INT,
            MATERIAL_PARAMETER_TYPE_INT2,
            MATERIAL_PARAMETER_TYPE_INT3,
            MATERIAL_PARAMETER_TYPE_INT4
        } type;

        Parameter() : type(MATERIAL_PARAMETER_TYPE_NONE) {}

        template <size_t Size>
        explicit Parameter(FixedArray<Float, Size> &&v)
            : Parameter(v.Data(), Size)
        {
        }
        
        explicit Parameter(const Float *v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_FLOAT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.float_values, v, count * sizeof(Float));
        }
        
        Parameter(Float value)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT)
        {
            std::memcpy(values.float_values, &value, sizeof(Float));
        }

        Parameter(const Vector2 &xy)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT2)
        {
            std::memcpy(values.float_values, &xy.values, 2 * sizeof(Float));
        }

        Parameter(const Vector3 &xyz)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT3)
        {
            std::memcpy(values.float_values, &xyz.values, 3 * sizeof(Float));
        }

        Parameter(const Vector4 &xyzw)
            : type(MATERIAL_PARAMETER_TYPE_FLOAT4)
        {
            std::memcpy(values.float_values, &xyzw.values, 4 * sizeof(Float));
        }

        template <size_t Size>
        explicit Parameter(FixedArray<Int32, Size> &&v)
            : Parameter(v.Data(), Size)
        {
        }
        
        explicit Parameter(const Int32 *v, SizeType count)
            : type(Type(MATERIAL_PARAMETER_TYPE_INT + (count - 1)))
        {
            AssertThrow(count >= 1 && count <= 4);

            std::memcpy(values.int_values, v, count * sizeof(Int32));
        }

        explicit Parameter(const Parameter &other)
            : type(other.type)
        {
            std::memcpy(&values, &other.values, sizeof(values));
        }

        Parameter &operator=(const Parameter &other)
        {
            type = other.type;
            std::memcpy(&values, &other.values, sizeof(values));

            return *this;
        }

        ~Parameter() = default;

        bool IsIntType() const
            { return type >= MATERIAL_PARAMETER_TYPE_INT && type <= MATERIAL_PARAMETER_TYPE_INT4; }

        bool IsFloatType() const
            { return type >= MATERIAL_PARAMETER_TYPE_FLOAT && type <= MATERIAL_PARAMETER_TYPE_FLOAT4; }

        UInt Size() const
        {
            if (type == MATERIAL_PARAMETER_TYPE_NONE) {
                return 0u;
            }

            if (type >= MATERIAL_PARAMETER_TYPE_INT) {
                return UInt(type - MATERIAL_PARAMETER_TYPE_INT) + 1;
            }

            return UInt(type);
        }

        void Copy(unsigned char dst[4]) const
        {
            std::memcpy(dst, &values, Size());
        }

        bool operator==(const Parameter &other) const
        {
            return values.ptr == other.values.ptr
                || !std::memcmp(&values, &other.values, sizeof(values));
        }

        HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(int(type));

            if (IsIntType()) {
                hc.Add(values.int_values[0]);
                hc.Add(values.int_values[1]);
                hc.Add(values.int_values[2]);
                hc.Add(values.int_values[3]);
            } else if (IsFloatType()) {
                hc.Add(values.float_values[0]);
                hc.Add(values.float_values[1]);
                hc.Add(values.float_values[2]);
                hc.Add(values.float_values[3]);
            }

            return hc;
        }
    };

    enum State {
        MATERIAL_STATE_CLEAN,
        MATERIAL_STATE_DIRTY
    };
    
    enum MaterialKey : UInt64
    {
        MATERIAL_KEY_NONE = 0,

        // basic
        MATERIAL_KEY_ALBEDO                 = 1 << 0,
        MATERIAL_KEY_METALNESS              = 1 << 1,
        MATERIAL_KEY_ROUGHNESS              = 1 << 2,
        MATERIAL_KEY_TRANSMISSION           = 1 << 3,
        MATERIAL_KEY_EMISSIVE               = 1 << 4,  // UNUSED
        MATERIAL_KEY_SPECULAR               = 1 << 5,  // UNUSED
        MATERIAL_KEY_SPECULAR_TINT          = 1 << 6,  // UNUSED
        MATERIAL_KEY_ANISOTROPIC            = 1 << 7,  // UNUSED
        MATERIAL_KEY_SHEEN                  = 1 << 8,  // UNUSED
        MATERIAL_KEY_SHEEN_TINT             = 1 << 9,  // UNUSED
        MATERIAL_KEY_CLEARCOAT              = 1 << 10,  // UNUSED
        MATERIAL_KEY_CLEARCOAT_GLOSS        = 1 << 11, // UNUSED
        MATERIAL_KEY_SUBSURFACE             = 1 << 12,  // UNUSED
        MATERIAL_KEY_FLIP_UV                = 1 << 13,
        MATERIAL_KEY_UV_SCALE               = 1 << 14,
        MATERIAL_KEY_PARALLAX_HEIGHT        = 1 << 15,
        MATERIAL_KEY_RESERVED1              = 1 << 16,
        MATERIAL_KEY_RESERVED2              = 1 << 17,

        // terrain
        MATERIAL_KEY_TERRAIN_LEVEL_0_HEIGHT = 1 << 18,
        MATERIAL_KEY_TERRAIN_LEVEL_1_HEIGHT = 1 << 19,
        MATERIAL_KEY_TERRAIN_LEVEL_2_HEIGHT = 1 << 20,
        MATERIAL_KEY_TERRAIN_LEVEL_3_HEIGHT = 1 << 21
    };

    using ParameterTable = EnumOptions<MaterialKey, Parameter, max_parameters>;
    using TextureSet = EnumOptions<TextureKey, Handle<Texture>, max_textures>;

    Material(const String &name = String::empty);
    Material(const Material &other) = delete;
    Material &operator=(const Material &other) = delete;
    ~Material();

    ShaderDataState GetShaderDataState() const
        { return m_shader_data_state; }

    void SetShaderDataState(ShaderDataState state)
        { m_shader_data_state = state; }

    ParameterTable &GetParameters()
        { return m_parameters; }

    const ParameterTable &GetParameters() const
        { return m_parameters; }

    const Parameter &GetParameter(MaterialKey key) const
        { return m_parameters.Get(key); }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
        { return m_parameters.Get(key).values.float_values[0]; }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<T>, int>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
        { return m_parameters.Get(key).values.int_values[0]; }

    template <class T>
    typename std::enable_if_t<std::is_same_v<std::decay_t<decltype(T::values[0])>, float>, std::decay_t<T>>
    GetParameter(MaterialKey key) const
    {
        static_assert(sizeof(T::values) <= sizeof(Parameter::values));

        T result;
        std::memcpy(&result.values[0], &m_parameters.Get(key).values.float_values[0], sizeof(float) * std::size(result.values));
        return result;
    }

    /*! \brief Set a parameter on this material with the given key and value
     * @param key The key of the parameter to be set
     * @param value The value of the parameter to be set
     */
    void SetParameter(MaterialKey key, const Parameter &value);

    /*! \brief Set all parameters back to their default values. */
    void ResetParameters();

    /*! \brief Sets the texture with the given key on this Material.
     * If the Material has already been initialized, the Texture is initialized.
     * Otherwise, it will be initialized when the Material's init callback is fired off.
     * @param key The texture slot to set the texture on
     * @param texture A Texture resource
     */
    void SetTexture(TextureKey key, Handle<Texture> &&texture);

    TextureSet &GetTextures()
        { return m_textures; }

    const TextureSet &GetTextures() const
        { return m_textures; }

    /*! \brief Return a pointer to a Texture set on this Material by the given
     * texture key. If no Texture was set, nullptr is returned.
     * @param key The key of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    Texture *GetTexture(TextureKey key);

    /*! \brief Return a pointer to a Texture set on this Material by the given
     * texture key. If no Texture was set, nullptr is returned.
     * @param key The key of the texture to find
     * @returns Pointer to the found Texture, or nullptr.
     */
    const Texture *GetTexture(TextureKey key) const;

    Bucket GetBucket() const
        { return m_render_attributes.bucket; }

    void SetBucket(Bucket bucket)
        { m_render_attributes.bucket = bucket; }

    bool IsAlphaBlended() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING); }

    void SetIsAlphaBlended(bool is_alpha_blended)
    {
        if (is_alpha_blended) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING;
        }
    }

    bool IsDepthWriteEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE); }

    void SetIsDepthWriteEnabled(bool is_depth_write_enabled)
    {
        if (is_depth_write_enabled) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE;
        }
    }

    bool IsDepthTestEnabled() const
        { return bool(m_render_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST); }

    void SetIsDepthTestEnabled(bool is_depth_test_enabled)
    {
        if (is_depth_test_enabled) {
            m_render_attributes.flags |= MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
        } else {
            m_render_attributes.flags &= ~MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
        }
    }

    FaceCullMode GetFaceCullMode() const
        { return m_render_attributes.cull_faces; }

    void SetFaceCullMode(FaceCullMode cull_mode)
        { m_render_attributes.cull_faces = cull_mode; }

    MaterialAttributes &GetRenderAttributes()
        { return m_render_attributes; }

    const MaterialAttributes &GetRenderAttributes() const
        { return m_render_attributes; }

    void Init(Engine *engine);
    void Update(Engine *engine);

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_parameters.GetHashCode());

        return hc;
    }

private:
    static constexpr UInt max_textures_to_set = MathUtil::Min(
        max_textures,
        MaterialShaderData::max_bound_textures,
        HYP_FEATURES_BINDLESS_TEXTURES
            ? DescriptorSet::max_bindless_resources
            : DescriptorSet::max_material_texture_samplers
    );

    void EnqueueRenderUpdates();
    void EnqueueTextureUpdate(TextureKey key);
    void EnqueueDescriptorSetCreate();
    void EnqueueDescriptorSetDestroy();

    ParameterTable m_parameters;
    TextureSet m_textures;

    MaterialShaderData m_shader_data;
    mutable ShaderDataState m_shader_data_state;

    MaterialAttributes m_render_attributes;

    FixedArray<DescriptorSet *, max_frames_in_flight> m_descriptor_sets;
};

class MaterialGroup : public EngineComponentBase<STUB_CLASS(MaterialGroup)>
{
public:
    MaterialGroup();
    MaterialGroup(const MaterialGroup &other) = delete;
    MaterialGroup &operator=(const MaterialGroup &other) = delete;
    ~MaterialGroup();

    void Init(Engine *engine);
    void Add(const std::string &name, Handle<Material> &&material);
    bool Remove(const std::string &name);

    Handle<Material> &Get(const std::string &name)
        { return m_materials[name]; }

    const Handle<Material> &Get(const std::string &name) const
        { return m_materials.at(name); }

    bool Has(const std::string &name) const
        { return m_materials.find(name) != m_materials.end(); }

private:
    std::unordered_map<std::string, Handle<Material>> m_materials;
};

} // namespace hyperion::v2

#endif
