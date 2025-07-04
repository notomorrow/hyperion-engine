/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Shader.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

ShaderManager* ShaderManager::GetInstance()
{
    return g_shader_manager;
}

ShaderRef ShaderManager::GetOrCreate(const ShaderDefinition& definition)
{
    HYP_NAMED_SCOPE("Get shader from cache or create");

    const auto ensure_contains_properties = [](const ShaderProperties& expected, const ShaderProperties& received) -> bool
    {
        if (!received.HasRequiredVertexAttributes(expected.GetRequiredVertexAttributes()))
        {
            return false;
        }

        for (const ShaderProperty& property : expected.GetPropertySet())
        {
            if (!received.Has(property))
            {
                return false;
            }
        }

        return true;
    };

    RC<ShaderMapEntry> entry;
    bool should_add_entry_to_cache = false;

    { // pulling value from cache if it exists
        Mutex::Guard guard(m_mutex);

        auto it = m_map.Find(definition);

        if (it != m_map.End())
        {
            entry = it->second;
        }
        else
        {
            should_add_entry_to_cache = true;
        }
    }

    if (entry != nullptr)
    {
        constexpr int max_spins = 1024;
        int num_spins = 0;

        // loading from another thread -- wait until state is no longer LOADING
        while (entry->state.Get(MemoryOrder::SEQUENTIAL) == ShaderMapEntry::State::LOADING)
        {
            // sanity check - should never happen
            AssertThrow(entry->loading_thread_id != Threads::CurrentThreadID());

            if (num_spins == max_spins)
            {
                HYP_LOG(Shader, Warning,
                    "Shader {} is loading for too long! Skipping reuse attempt and loading on this thread. (Loading thread: {}, current thread: {})",
                    definition.GetName(),
                    entry->loading_thread_id.GetName(),
                    Threads::CurrentThreadID().GetName());

                entry = MakeRefCountedPtr<ShaderMapEntry>();
                entry->state.Set(ShaderMapEntry::State::LOADING, MemoryOrder::SEQUENTIAL);
                entry->loading_thread_id = Threads::CurrentThreadID();

                should_add_entry_to_cache = false;

                break;
            }

            Threads::Sleep(0);

            num_spins++;
        }

        if (ShaderRef shader = entry->shader.Lock())
        {
            if (ensure_contains_properties(definition.GetProperties(), shader->GetCompiledShader()->GetProperties()))
            {
                return shader;
            }
            else
            {
                HYP_LOG(Shader, Error,
                    "Loaded shader from cache (Name: {}, Properties: {}) does not contain the requested properties!\n\tRequested: {}",
                    definition.GetName(),
                    shader->GetCompiledShader()->GetProperties().ToString(),
                    definition.GetProperties().ToString());
            }
        }

        // @TODO: remove bad value from cache?
    }

    RC<CompiledShader> compiled_shader = MakeRefCountedPtr<CompiledShader>();

    if (!entry)
    {
        entry = MakeRefCountedPtr<ShaderMapEntry>();
        entry->compiled_shader = compiled_shader;
        entry->state.Set(ShaderMapEntry::State::LOADING, MemoryOrder::SEQUENTIAL);
        entry->loading_thread_id = Threads::CurrentThreadID();
    }

    if (should_add_entry_to_cache)
    {
        AssertDebug(entry != nullptr);

        Mutex::Guard guard(m_mutex);

        // double check, as we don't want to overwrite an entry, potentially invalidating references to the compiled shader.
        auto it = m_map.Find(definition);

        if (it == m_map.End())
        {
            m_map.Insert(definition, entry);
        }
    }

    ShaderRef shader;

    { // loading / compilation of shader (outside of mutex lock)

        bool is_valid_compiled_shader = true;

        is_valid_compiled_shader &= g_engine->GetShaderCompiler().GetCompiledShader(
            definition.GetName(),
            definition.GetProperties(),
            *compiled_shader);

        is_valid_compiled_shader &= compiled_shader->GetDefinition().IsValid();

        AssertThrowMsg(
            is_valid_compiled_shader,
            "Failed to get compiled shader with name %s and props hash %llu!\n",
            definition.GetName().LookupString(),
            definition.GetProperties().GetHashCode().Value());

        shader = g_rendering_api->MakeShader(std::move(compiled_shader));

#ifdef HYP_DEBUG_MODE
        AssertThrow(ensure_contains_properties(definition.GetProperties(), shader->GetCompiledShader()->GetDefinition().GetProperties()));
#endif

        DeferCreate(shader);

        // Update the entry
        entry->shader = shader;
        entry->state.Set(ShaderMapEntry::State::LOADED, MemoryOrder::SEQUENTIAL);
    }

    return shader;
}

ShaderRef ShaderManager::GetOrCreate(
    Name name,
    const ShaderProperties& props)
{
    return GetOrCreate(ShaderDefinition {
        name,
        props });
}

SizeType ShaderManager::CalculateMemoryUsage() const
{
    HYP_NAMED_SCOPE("ShaderManager::CalculateMemoryUsage");

    SizeType total_memory_usage = 0;

    Mutex::Guard guard(m_mutex);

    for (const auto& it : m_map)
    {
        total_memory_usage += sizeof(it.first);
        total_memory_usage += sizeof(it.second);

        if (const RC<ShaderMapEntry>& entry = it.second)
        {
            if (ShaderRef shader = entry->shader.Lock())
            {
                for (const ByteBuffer& byte_buffer : shader->GetCompiledShader()->GetModules())
                {
                    total_memory_usage += byte_buffer.Size();
                }
            }
        }
    }

    return total_memory_usage;
}

} // namespace hyperion
