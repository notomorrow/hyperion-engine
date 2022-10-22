#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <core/Containers.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

#include <set>
#include <string>
#include <vector>

namespace hyperion::v2 {

class Engine;

using renderer::ShaderModule;

struct CompiledShader
{
    UInt64 props_hash;
    FixedArray<ByteBuffer, ShaderModule::Type::MAX> modules;

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(props_hash);
        hc.Add(modules.GetHashCode());

        return hc;
    }
};

struct CompiledShaderBatch
{
    DynArray<CompiledShader> compiled_shaders;

    HashCode GetHashCode() const
    {
        return compiled_shaders.GetHashCode();
    }
};

class ShaderProps
{
public:
    using Iterator = DynArray<String>::Iterator;
    using ConstIterator = DynArray<String>::ConstIterator;

    bool Get(const String &key) const
    {
        const auto it = m_props.Find(key);

        if (it == m_props.End()) {
            return false;
        }

        return true;
    }

    ShaderProps &Set(const String &key, bool value)
    {
        if (!value) {
            m_props.Erase(key);
        } else {
            if (!m_props.Contains(key)) {
                m_props.PushBack(key);
            }
            //m_props.Insert(key);
        }

        return *this;
    }

    SizeType Size() const
        { return m_props.Size(); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_props) {
            hc.Add(it);
        }

        return hc;
    }

    HYP_DEF_STL_BEGIN_END(
        m_props.Begin(),
        m_props.End()
    )

private:
    DynArray<String> m_props;
    // FlatSet<String> m_props;
};

class ShaderCompiler
{
public:
    struct SourceFile
    {
        String path;

        HashCode GetHashCode() const
        {
            // TODO: should it include timestamp of last modified?
            HashCode hc;
            hc.Add(path);

            return hc;
        }
    };

    struct Bundle // combination of shader files, .frag, .vert etc.
    {
        String name;
        FlatMap<ShaderModule::Type, SourceFile> sources;
        ShaderProps props; // permutations
    };

    void AddBundle(Bundle &&bundle)
    {
        m_bundles.PushBack(std::move(bundle));
    }

    void CompileBundles();

    bool GetCompiledShader(
        Engine *engine,
        const String &name,
        const ShaderProps &props,
        CompiledShader &out
    );

private:
    bool CompileBundle(Bundle &bundle);

    DynArray<Bundle> m_bundles;
};

} // namespace hyperion::v2

#endif