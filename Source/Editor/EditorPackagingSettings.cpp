/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorPackagingSettings.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Field.hpp>

#include <EditorPackagingSettings.generated.inl>

namespace Hyperion {

namespace {

static const Pair<TargetPlatform, ANSIStringView> s_targetPlatformNames[] = {
    { TargetPlatform::Windows, "windows" },
    { TargetPlatform::Mac, "mac" },
    { TargetPlatform::Linux, "linux" },
    { TargetPlatform::Android, "android" },
    { TargetPlatform::IOS, "ios" },
    { TargetPlatform::Web, "web" }
};

static const Pair<TargetPlatform, void (*)(PackagingOptions&)> s_platformDefaults[] = {
    {
        TargetPlatform::Web,
        [](PackagingOptions& options)
        {
            // Web needs to be limited to reduce download size.
            options.content.maxTextureSize = 256;
        }
    },
    {
        TargetPlatform::IOS,
        [](PackagingOptions& options)
        {
            options.content.maxTextureSize = 512;
        }
    },
    {
        TargetPlatform::Android,
        [](PackagingOptions& options)
        {
            options.content.maxTextureSize = 512;
        }
    }
};

String MemberPath(const String& prefix, const IMember& member)
{
    const String name(member.GetName().LookupString());

    return prefix.Any() ? prefix + "." + name : name;
}

const Class* NestedClass(const IMember& member)
{
    const Class* cls = member.GetTypeInfo().GetClass();

    return cls != nullptr && cls->IsStructType() ? cls : nullptr;
}

String ValueToHMF(const BoxedValue& value, const IMember& member)
{
    String text;
    BoxedToHMF(value, text, &member.GetTypeInfo());

    return text;
}

void CollectDifferences(
    const Class* cls,
    const BoxedValue& defaults,
    const BoxedValue& options,
    const String& prefix,
    Array<String>& outPaths)
{
    for (const IMember& member : cls->GetMembers(MemberType::Field, false))
    {
        const Field& field = static_cast<const Field&>(member);
        const String path = MemberPath(prefix, member);

        const BoxedValue defaultValue = field.Get(defaults);
        const BoxedValue value = field.Get(options);

        if (const Class* nestedClass = NestedClass(member))
        {
            CollectDifferences(nestedClass, defaultValue, value, path, outPaths);
        }
        else if (ValueToHMF(defaultValue, member) != ValueToHMF(value, member))
        {
            outPaths.PushBack(path);
        }
    }
}

void CopyOverriddenFields(
    const Class* cls,
    BoxedValue& target,
    const BoxedValue& source,
    const String& prefix,
    const Array<String>& paths)
{
    for (const IMember& member : cls->GetMembers(MemberType::Field, false))
    {
        const Field& field = static_cast<const Field&>(member);
        const String path = MemberPath(prefix, member);

        if (paths.Contains(path))
        {
            field.Set(target, field.Get(source));
        }
        else if (const Class* nestedClass = NestedClass(member))
        {
            BoxedValue nested = field.Get(target);
            CopyOverriddenFields(nestedClass, nested, field.Get(source), path, paths);

            field.Set(target, nested);
        }
    }
}

Array<String> FindOverriddenFields(const PackagingOptions& defaults, const PackagingOptions& options)
{
    Array<String> paths;
    CollectDifferences(
        PackagingOptions::StaticClass(),
        BoxedValue(defaults),
        BoxedValue(options),
        String::empty,
        paths);

    return paths;
}

PackagingOptions MergeOverride(const PackagingOptions& defaults, const PackagingPlatformOverride& platformOverride)
{
    if (platformOverride.overriddenFields.Empty())
    {
        return defaults;
    }

    BoxedValue merged { defaults };

    CopyOverriddenFields(
        PackagingOptions::StaticClass(),
        merged,
        BoxedValue(platformOverride.options),
        String::empty,
        platformOverride.overriddenFields);

    return merged.Get<PackagingOptions>();
}

} // namespace

TargetPlatform GetHostTargetPlatform()
{
#if defined(HYP_WINDOWS)
    return TargetPlatform::Windows;
#elif defined(HYP_MACOS)
    return TargetPlatform::Mac;
#else
    return TargetPlatform::Linux;
#endif
}

ANSIStringView GetTargetPlatformName(TargetPlatform platform)
{
    for (const auto& entry : s_targetPlatformNames)
    {
        if (entry.first == platform)
        {
            return entry.second;
        }
    }

    return {};
}

bool ParseTargetPlatform(ANSIStringView name, TargetPlatform& outPlatform)
{
    for (const auto& entry : s_targetPlatformNames)
    {
        if (entry.second.Length() == name.Length() && entry.second == name)
        {
            outPlatform = entry.first;

            return true;
        }
    }

    return false;
}

#pragma region PackagingSettings

Handle<PackagingSettings> PackagingSettings::CreateDefault()
{
    Handle<PackagingSettings> settings = MakeHandle<PackagingSettings>();

    for (const auto& entry : s_platformDefaults)
    {
        settings->AddPlatformOverride(entry.first);
    }

    return settings;
}

Handle<PackagingPlatformOverride> PackagingSettings::AddPlatformOverride(TargetPlatform platform)
{
    for (const Handle<PackagingPlatformOverride>& platformOverride : m_platformOverrides)
    {
        if (platformOverride.IsValid() && platformOverride->platform == platform)
        {
            return platformOverride;
        }
    }

    Handle<PackagingPlatformOverride> platformOverride = MakeHandle<PackagingPlatformOverride>(platform, defaults);

    for (const auto& entry : s_platformDefaults)
    {
        if (entry.first == platform)
        {
            entry.second(platformOverride->options);
        }
    }

    platformOverride->overriddenFields = FindOverriddenFields(defaults, platformOverride->options);

    m_platformOverrides.PushBack(platformOverride);

    return platformOverride;
}

void PackagingSettings::RemovePlatformOverride(TargetPlatform platform)
{
    for (auto it = m_platformOverrides.Begin(); it != m_platformOverrides.End(); ++it)
    {
        if (it->IsValid() && (*it)->platform == platform)
        {
            m_platformOverrides.Erase(it);

            return;
        }
    }
}

void PackagingSettings::ApplyDefaultsToOverrides()
{
    for (const Handle<PackagingPlatformOverride>& platformOverride : m_platformOverrides)
    {
        if (platformOverride.IsValid())
        {
            platformOverride->options = MergeOverride(defaults, *platformOverride);
        }
    }
}

void PackagingSettings::UpdateOverriddenFields()
{
    for (const Handle<PackagingPlatformOverride>& platformOverride : m_platformOverrides)
    {
        if (platformOverride.IsValid())
        {
            platformOverride->overriddenFields = FindOverriddenFields(defaults, platformOverride->options);
        }
    }
}

PackagingOptions PackagingSettings::Resolve(TargetPlatform platform) const
{
    for (const Handle<PackagingPlatformOverride>& platformOverride : m_platformOverrides)
    {
        if (platformOverride.IsValid() && platformOverride->platform == platform)
        {
            return MergeOverride(defaults, *platformOverride);
        }
    }

    return defaults;
}

#pragma endregion PackagingSettings

} // namespace Hyperion
