#pragma once
#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Uuid.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class Object;
} // namespace dotnet

HYP_ENUM()
enum ScriptCompileStatus : uint32
{
    SCS_UNINITIALIZED = 0x0,
    SCS_COMPILED = 0x1,
    SCS_DIRTY = 0x2,
    SCS_PROCESSING = 0x4,
    SCS_ERRORED = 0x8
};

HYP_MAKE_ENUM_FLAGS(ScriptCompileStatus)

HYP_ENUM()
enum ScriptLanguage : uint32
{
    SL_INVALID = uint32(-1),

    SL_HYPSCRIPT = 0,
    SL_CSHARP = 1
};

struct ScriptDesc
{
    FilePath path;
};

static constexpr SizeType scriptMaxPathLength = 1024;
static constexpr SizeType scriptMaxClassNameLength = 1024;

HYP_STRUCT()
struct ScriptData
{
    HYP_FIELD(Serialize)
    UUID uuid;

    HYP_FIELD(Serialize)
    ScriptLanguage language = SL_HYPSCRIPT;

    HYP_FIELD(Serialize)
    char path[scriptMaxPathLength];

    HYP_FIELD(Serialize)
    char assemblyPath[scriptMaxPathLength];

    HYP_FIELD(Serialize)
    char className[scriptMaxClassNameLength];

    HYP_FIELD(Serialize)
    uint32 compileStatus;

    HYP_FIELD(Serialize)
    int32 hotReloadVersion;

    HYP_FIELD(Serialize)
    uint64 lastModifiedTimestamp;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(uuid);
        hashCode.Add(language);
        hashCode.Add(&path[0]);
        hashCode.Add(&assemblyPath[0]);
        hashCode.Add(&className[0]);
        hashCode.Add(compileStatus);

        return hashCode;
    }
};

static_assert(std::is_standard_layout_v<ScriptData>, "ScriptData struct must be standard layout");
static_assert(std::is_trivially_copyable_v<ScriptData>, "ScriptData struct must be a trivial type");

} // namespace hyperion
