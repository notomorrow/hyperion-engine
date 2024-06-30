#ifndef HYPERION_SCRIPT_HPP
#define HYPERION_SCRIPT_HPP

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/UUID.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/filesystem/FilePath.hpp>

#include <HashCode.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class Object;
} // namespace dotnet

enum class CompiledScriptState : uint32
{
    UNINITIALIZED   = 0x0,
    COMPILED        = 0x1,
    DIRTY           = 0x2,
    PROCESSING      = 0x4,
    ERRORED         = 0x8
};

HYP_MAKE_ENUM_FLAGS(CompiledScriptState)

struct CompiledScript
{
    UniquePtr<dotnet::Assembly> assembly;
    UniquePtr<dotnet::Object>   object;
};

struct ScriptDesc
{
    FilePath    path;
};

static constexpr SizeType script_max_path_length = 1024;
static constexpr SizeType script_max_class_name_length = 1024;

struct ManagedScript
{
    UUID    uuid;
    char    path[script_max_path_length];
    char    assembly_path[script_max_path_length];
    char    class_name[script_max_class_name_length];
    uint32  state;
    int32   hot_reload_version;
    uint64  last_modified_timestamp;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(uuid);
        hash_code.Add(&path[0]);
        hash_code.Add(&assembly_path[0]);
        hash_code.Add(&class_name[0]);
        hash_code.Add(state);

        return hash_code;
    }
};

static_assert(std::is_standard_layout_v<ManagedScript>, "ManagedScript struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ManagedScript>, "ManagedScript struct must be a trivial type");
static_assert(sizeof(ManagedScript) == 3104, "ManagedScript struct size must match C# struct size");

class HYP_API Script
{
public:
    Script(const ScriptDesc &desc);
    Script(const Script &other)                 = delete;
    Script &operator=(const Script &other)      = delete;
    Script(Script &&other) noexcept             = delete;
    Script &operator=(Script &&other) noexcept  = delete;
    ~Script();

    HYP_NODISCARD HYP_FORCE_INLINE
    const ScriptDesc &GetDescriptor() const
        { return m_desc; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const CompiledScript &GetCompiledScript() const
        { return m_compiled_script; }

    HYP_NODISCARD HYP_FORCE_INLINE
    ManagedScript &GetManagedScript()
        { return m_managed_script; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const ManagedScript &GetManagedScript() const
        { return m_managed_script; }

    HYP_NODISCARD
    EnumFlags<CompiledScriptState> GetState() const;

private:
    ScriptDesc          m_desc;
    CompiledScript      m_compiled_script;
    ManagedScript       m_managed_script;
};

} // namespace hyperion

#endif