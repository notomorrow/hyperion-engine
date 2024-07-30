/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Config.hpp>
#include <Engine.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <asset/Assets.hpp>
#include <asset/ByteWriter.hpp>

namespace hyperion {

#pragma region Option

int Option::GetInt() const
{
    if (auto *ptr = TryGet<int>()) {
        return *ptr;
    }

    if (auto *ptr = TryGet<float>()) {
        return static_cast<int>(*ptr);
    }

    if (auto *ptr = TryGet<bool>()) {
        return static_cast<int>(*ptr);
    }

    if (auto *ptr = TryGet<String>()) {
        return StringUtil::Parse<int>(*ptr, 0);
    }

    return 0;
}

float Option::GetFloat() const
{
    if (auto *ptr = TryGet<int>()) {
        return static_cast<float>(*ptr);
    }

    if (auto *ptr = TryGet<float>()) {
        return *ptr;
    }

    if (auto *ptr = TryGet<bool>()) {
        return static_cast<float>(*ptr);
    }

    if (auto *ptr = TryGet<String>()) {
        return StringUtil::Parse<float>(*ptr, 0);
    }

    return 0.0f;
}

bool Option::GetBool() const
{
    if (auto *ptr = TryGet<int>()) {
        return static_cast<bool>(*ptr);
    }

    if (auto *ptr = TryGet<float>()) {
        return static_cast<bool>(*ptr);
    }

    if (auto *ptr = TryGet<bool>()) {
        return *ptr;
    }

    if (auto *ptr = TryGet<String>()) {
        const String trimmed = ptr->Trimmed().ToLower();

        return trimmed == "true" || GetInt() != 0;
    }

    return false;
}

String Option::GetString() const
{
    if (auto *ptr = TryGet<int>()) {
        return HYP_FORMAT("{}", *ptr);
    }

    if (auto *ptr = TryGet<float>()) {
        return HYP_FORMAT("{}", *ptr);
    }

    if (auto *ptr = TryGet<bool>()) {
        return HYP_FORMAT("{}", *ptr);
    }

    if (auto *ptr = TryGet<String>()) {
        return *ptr;
    }

    return String::empty;
}

#pragma endregion Option

#pragma region Configuration

const FlatMap<OptionName, String> Configuration::option_name_strings = {
    { CONFIG_DEBUG_MODE, "DebugMode" },
    { CONFIG_SHADER_COMPILATION, "ShaderCompilation" },
    { CONFIG_RT_ENABLED, "RTEnabled" },
    { CONFIG_RT_REFLECTIONS, "RTReflections" },
    { CONFIG_RT_GI, "RTGlobalIllumination" },
    { CONFIG_RT_GI_DEBUG_PROBES, "DebugDDGIProbes" },
    { CONFIG_PATHTRACER,  "PathTracer" },
    { CONFIG_SSR, "ScreenSpaceReflections" },
    { CONFIG_ENV_GRID_REFLECTIONS, "EnvGridReflections" },
    { CONFIG_ENV_GRID_GI, "EnvGridGlobalIllumination" },
    { CONFIG_ENV_GRID_GI_MODE, "EnvGridGlobalIlluminationMode" },
    { CONFIG_HBAO, "HBAO" },
    { CONFIG_HBIL, "HBIL" },
    { CONFIG_TEMPORAL_AA, "TemporalAA" },
    { CONFIG_LIGHT_RAYS, "LightRays" },
    { CONFIG_DEBUG_SSR, "DebugSSR" },
    { CONFIG_DEBUG_HBAO, "DebugHBAO" },
    { CONFIG_DEBUG_HBIL, "DebugHBIL" },
    { CONFIG_DEBUG_REFLECTIONS, "DebugReflections" },
    { CONFIG_DEBUG_IRRADIANCE, "DebugIrradiance" },
    { CONFIG_DEBUG_REFLECTION_PROBES, "DebugReflectionProbes" },
    { CONFIG_DEBUG_ENV_GRID_PROBES, "DebugEnvGridProbes" }
};

OptionName Configuration::StringToOptionName(const String &str)
{
    for (auto &it : option_name_strings) {
        if (it.second == str) {
            return it.first;
        }
    }

    return CONFIG_NONE;
}

String Configuration::OptionNameToString(OptionName option)
{
    const auto it = option_name_strings.Find(option);

    if (it == option_name_strings.End()) {
        return "Unknown";
    }

    return it->second;
}

bool Configuration::IsRTOption(OptionName option)
{
    return option == CONFIG_RT_ENABLED
        || option == CONFIG_RT_REFLECTIONS
        || option == CONFIG_RT_GI
        || option == CONFIG_RT_GI_DEBUG_PROBES
        || option == CONFIG_PATHTRACER;
}

Configuration::Configuration() = default;

bool Configuration::LoadFromDefinitionsFile()
{
    const INIFile definitions(g_asset_manager->GetBasePath() / "Config.ini");

    if (!definitions.IsValid()) {
        return false;
    }

    for (const auto &it : definitions.GetSections()) {
        for (const auto &option_it : it.second) {
            const OptionName option_name = StringToOptionName(option_it.first);
            const INIFile::Value &option_value = option_it.second;

            if (option_name == CONFIG_NONE || option_name >= CONFIG_MAX) {
                HYP_LOG(Config, LogLevel::WARNING, "{}: Unknown config option", option_it.first);

                continue;
            }

            Option value;

            if (IsRTOption(option_name) && !g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()) {
                value = false;
            } else {
                union { int i; float f; } tmp_value;

                if (option_value.GetValue().name == "true") {
                    value = true;
                } else if (option_value.GetValue().name == "false") {
                    value = false;
                } else if (StringUtil::Parse(option_it.first.Data(), &tmp_value.i)) {
                    value = tmp_value.i;
                } else if (StringUtil::Parse(option_it.first.Data(), &tmp_value.f)) {
                    value = tmp_value.f;
                } else {
                    value = false;
                }
            }

            m_variables[option_name] = std::move(value);
        }
    }

    return true;
}

bool Configuration::SaveToDefinitionsFile()
{
    static const String default_section_name = "Default";
    
    // for (Option &option : m_variables) {
    //     if (!(option.GetFlags() & OptionFlags::SAVE)) {
    //         continue;
    //     }

    //     String value_string = "false";

    //     if (option.IsValid()) {
    //         value_string = option.GetString();
    //     }

    //     m_ini_file
    //         .GetSection(default_section_name)
    //         .Set()
    // }

    String str_result = "[Default]\n";

    for (uint index = CONFIG_NONE + 1; index < CONFIG_MAX; index++) {
        const Option &option = m_variables[index];

        if (!(option.GetFlags() & OptionFlags::SAVE)) {
            continue;
        }

        String value_string = "false";

        if (option.IsValid()) {
            value_string = option.GetString();
        }

        str_result += OptionNameToString(OptionName(index)) + " = " + value_string + "\n";
    }

    const String path = g_asset_manager->GetBasePath() / "Config.ini";

    FileByteWriter writer(path.Data());

    if (!writer.IsOpen()) {
        return false;
    }

    writer.Write(str_result.Data(), str_result.Size());
    writer.Close();

    return true;
}

void Configuration::SetToDefaultConfiguration()
{
    m_variables = { };

#ifdef HYP_DEBUG_MODE
    m_variables[CONFIG_DEBUG_MODE] = Option(true, false);
#endif

#ifdef HYP_GLSLANG
    m_variables[CONFIG_SHADER_COMPILATION] = Option(true, false);
#else
    m_variables[CONFIG_SHADER_COMPILATION] = Option(false, false);
#endif
    
    m_variables[CONFIG_RT_ENABLED] = Option(g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported() && g_engine->GetGPUDevice()->GetFeatures().IsRaytracingEnabled(), true);
    m_variables[CONFIG_RT_REFLECTIONS] = Option(m_variables[CONFIG_RT_ENABLED].GetBool(), true);
    m_variables[CONFIG_RT_GI] = Option(m_variables[CONFIG_RT_ENABLED].GetBool(), true);

    m_variables[CONFIG_HBAO] = Option(true, true);
    m_variables[CONFIG_HBIL] = Option(m_variables[CONFIG_HBAO].GetBool(), true);
    
    m_variables[CONFIG_SSR] = Option(!m_variables[CONFIG_RT_REFLECTIONS].GetBool(), true);

    m_variables[CONFIG_TEMPORAL_AA] = Option(true, true);

    m_variables[CONFIG_LIGHT_RAYS] = Option(true, true);

    m_variables[CONFIG_PATHTRACER] = Option(false, true);

    m_variables[CONFIG_DEBUG_SSR] = Option(false, true);
    m_variables[CONFIG_DEBUG_HBAO] = Option(false, true);
    m_variables[CONFIG_DEBUG_HBIL] = Option(false, true);
    m_variables[CONFIG_DEBUG_REFLECTIONS] = Option(false, true);
    m_variables[CONFIG_DEBUG_IRRADIANCE] = Option(false, true);
    m_variables[CONFIG_DEBUG_REFLECTION_PROBES] = Option(false, true);
    m_variables[CONFIG_DEBUG_ENV_GRID_PROBES] = Option(false, true);
}

#pragma endregion Configuration

} // namespace hyperion