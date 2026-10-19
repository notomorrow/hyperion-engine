/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorPackagingSettings.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <EditorPackagingSettings.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

static const Pair<TargetPlatform, ANSIStringView> s_targetPlatformNames[] = {
    { TargetPlatform::Windows, "windows" },
    { TargetPlatform::Mac, "mac" },
    { TargetPlatform::Linux, "linux" },
    { TargetPlatform::Android, "android" },
    { TargetPlatform::IOS, "ios" },
    { TargetPlatform::Web, "web" }
};

// options are compared and merged as JSON, so any nesting of reflected structs works without per-field code
JSON::Value OptionsToJSON(const PackagingOptions& options)
{
    JSON::Value json;

    if (Result result = BoxedToJSON(BoxedValue(options), json); result.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to convert packaging options to JSON: {}", result.GetError().GetMessage());
    }

    return json;
}

PackagingOptions OptionsFromJSON(const JSON::Value& json)
{
    BoxedValue boxed;

    if (Result result = BoxedFromJSON(json, TypeOf<PackagingOptions>(), boxed); result.HasError() || !boxed.Is<PackagingOptions>())
    {
        HYP_LOG(Editor, Error, "Failed to read packaging options from JSON");

        return {};
    }

    return boxed.Get<PackagingOptions>();
}

void CollectDifferences(const JSON::Value& defaults, const JSON::Value& options, const String& path, Array<String>& outPaths)
{
    if (defaults.IsObject() && options.IsObject())
    {
        const JSON::Object& optionsObject = options.AsObject();

        for (const auto& entry : defaults.AsObject())
        {
            auto optionsIt = optionsObject.Find(entry.first);

            if (optionsIt == optionsObject.End())
            {
                continue;
            }

            CollectDifferences(entry.second, optionsIt->second, path.Any() ? path + "." + entry.first : entry.first, outPaths);
        }

        return;
    }

    if (defaults.ToString(true) != options.ToString(true))
    {
        outPaths.PushBack(path);
    }
}

const JSON::Value* FindPath(const JSON::Value& root, const String& path)
{
    const JSON::Value* current = &root;

    for (const String& key : path.Split('.'))
    {
        if (!current->IsObject())
        {
            return nullptr;
        }

        auto it = current->AsObject().Find(key);

        if (it == current->AsObject().End())
        {
            return nullptr;
        }

        current = &it->second;
    }

    return current;
}

void SetPath(JSON::Value& root, const String& path, const JSON::Value& value)
{
    JSON::Value* current = &root;

    for (const String& key : path.Split('.'))
    {
        if (!current->IsObject())
        {
            return;
        }

        auto it = current->AsObject().Find(key);

        if (it == current->AsObject().End())
        {
            return;
        }

        current = &it->second;
    }

    *current = value;
}

PackagingOptions MergeOverride(const PackagingOptions& defaults, const PackagingPlatformOverride& platformOverride)
{
    if (platformOverride.overriddenFields.Empty())
    {
        return defaults;
    }

    JSON::Value merged = OptionsToJSON(defaults);
    const JSON::Value overrideJson = OptionsToJSON(platformOverride.options);

    for (const String& path : platformOverride.overriddenFields)
    {
        if (const JSON::Value* value = FindPath(overrideJson, path))
        {
            SetPath(merged, path, *value);
        }
    }

    return OptionsFromJSON(merged);
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
    const JSON::Value defaultsJson = OptionsToJSON(defaults);

    for (const Handle<PackagingPlatformOverride>& platformOverride : m_platformOverrides)
    {
        if (!platformOverride.IsValid())
        {
            continue;
        }

        platformOverride->overriddenFields.Clear();

        CollectDifferences(defaultsJson, OptionsToJSON(platformOverride->options), String(), platformOverride->overriddenFields);
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
