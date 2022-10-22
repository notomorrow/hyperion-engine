#ifndef HYPERION_V2_SHADER_COMPILER_H
#define HYPERION_V2_SHADER_COMPILER_H

#include <core/Containers.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

#include <set>
#include <string>
#include <vector>
#include <mutex>

namespace hyperion::v2 {

class Engine;
class DefinitionsFile;

using renderer::ShaderModule;

struct CompiledShader
{
    UInt64 props_hash = ~0u;
    FixedArray<ByteBuffer, ShaderModule::Type::MAX> modules;

    explicit operator bool() const
        { return IsValid(); }

    bool IsValid() const
    {
        return props_hash != ~0u
            && modules.Any([](const ByteBuffer &buffer) { return buffer.Size() != 0; });
    }

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
            m_props.Insert(key);
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
    FlatSet<String> m_props;
};

class ShaderCache
{
public:
    ShaderCache() = default;
    ShaderCache(const ShaderCache &other) = delete;
    ShaderCache &operator=(const ShaderCache &other) = delete;
    ShaderCache(ShaderCache &&other) noexcept = delete;
    ShaderCache &operator=(ShaderCache &&other) noexcept = delete;
    ~ShaderCache() = default;

    bool Get(const String &key, CompiledShaderBatch &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(key);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        out = it->second;

        return true;
    }

    bool GetShaderInstance(const String &key, UInt64 props_hash, CompiledShader &out) const
    {
        std::lock_guard guard(m_mutex);

        const auto it = m_compiled_shaders.Find(key);

        if (it == m_compiled_shaders.End()) {
            return false;
        }

        const auto props_it = it->second.compiled_shaders.FindIf([props_hash](const CompiledShader &item) {
            return item.props_hash == props_hash;
        });

        if (props_it == it->second.compiled_shaders.End()) {
            return false;
        }

        out = *props_it;

        return true;
    }

    void Set(const String &key, const CompiledShaderBatch &batch)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Set(key, batch);
    }

    void Set(const String &key, CompiledShaderBatch &&batch)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Set(key, std::move(batch));
    }

    void Remove(const String &key)
    {
        std::lock_guard guard(m_mutex);

        m_compiled_shaders.Erase(key);
    }

private:
    FlatMap<String, CompiledShaderBatch> m_compiled_shaders;
    mutable std::mutex m_mutex;
};

class ShaderCompiler
{
public:
    struct SourceFile
    {
        String path;

        HashCode GetHashCode() const
        {
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

    ShaderCompiler(Engine *engine);
    ShaderCompiler(const ShaderCompiler &other) = delete;
    ShaderCompiler &operator=(const ShaderCompiler &other) = delete;
    ~ShaderCompiler();

    void AddBundle(Bundle &&bundle)
    {
        m_bundles.PushBack(std::move(bundle));
    }
    bool CompileBundle(const Bundle &bundle);

    bool ReloadCompiledShaderBatch(const String &name);
    void LoadShaderDefinitions();

    CompiledShader GetCompiledShader(
        const String &name
    );

    CompiledShader GetCompiledShader(
        const String &name,
        const ShaderProps &props
    );

private:
    Engine *m_engine;
    DefinitionsFile *m_definitions;
    ShaderCache m_cache;
    DynArray<Bundle> m_bundles;
};

} // namespace hyperion::v2

#endif