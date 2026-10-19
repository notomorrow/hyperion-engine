/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

HYP_ENUM()
enum class TargetPlatform : uint32
{
    Windows = 0,
    Mac,
    Linux,
    Android,
    IOS,
    Web
};

EDITOR_API TargetPlatform GetHostTargetPlatform();
EDITOR_API ANSIStringView GetTargetPlatformName(TargetPlatform platform);
EDITOR_API bool ParseTargetPlatform(ANSIStringView name, TargetPlatform& outPlatform);

HYP_STRUCT()
struct ContentPackagingOptions
{
    HYP_STRUCT_BODY(ContentPackagingOptions);
    
    HYP_FIELD(Property = "MaxTextureSize", Serialize, Editor, Title = "Max Texture Size", Description = "Largest texture dimension in the cooked game, textures larger than this will be downscaled. Use 0 for no limit")
    uint32 maxTextureSize = 0;
};

HYP_STRUCT()
struct PackagingOptions
{
    HYP_STRUCT_BODY(PackagingOptions);

    HYP_FIELD(Property = "Content", Serialize, Editor)
    ContentPackagingOptions content;
};

HYP_CLASS()
class EDITOR_API PackagingPlatformOverride final : public ObjectBase
{
    HYP_OBJECT_BODY(PackagingPlatformOverride);

public:
    PackagingPlatformOverride() = default;

    PackagingPlatformOverride(TargetPlatform platform, const PackagingOptions& options)
        : platform(platform),
          options(options)
    {
    }

    HYP_METHOD()
    HYP_FORCE_INLINE TargetPlatform GetPlatform() const
    {
        return platform;
    }

    HYP_FIELD(Property = "Platform", Serialize)
    TargetPlatform platform = TargetPlatform::Windows;

    HYP_FIELD(Property = "Options", Serialize, Editor, Title = "Settings")
    PackagingOptions options;

    HYP_FIELD(Property = "OverriddenFields", Serialize)
    Array<String> overriddenFields;
};

HYP_CLASS()
class EDITOR_API PackagingSettings final : public ObjectBase
{
    HYP_OBJECT_BODY(PackagingSettings);

public:
    PackagingSettings() = default;

    static Handle<PackagingSettings> CreateDefault();

    HYP_METHOD()
    HYP_FORCE_INLINE const Array<Handle<PackagingPlatformOverride>>& GetPlatformOverrides() const
    {
        return m_platformOverrides;
    }

    //////////

    HYP_METHOD()
    Handle<PackagingPlatformOverride> AddPlatformOverride(TargetPlatform platform);

    HYP_METHOD()
    void RemovePlatformOverride(TargetPlatform platform);

    //////////

    HYP_METHOD()
    void ApplyDefaultsToOverrides();

    HYP_METHOD()
    void UpdateOverriddenFields();

    PackagingOptions Resolve(TargetPlatform platform) const;

    HYP_FIELD(Property = "Defaults", Serialize, Editor, Title = "Defaults")
    PackagingOptions defaults;

private:
    HYP_FIELD(Property = "PlatformOverrides", Serialize)
    Array<Handle<PackagingPlatformOverride>> m_platformOverrides;
};

} // namespace Hyperion
