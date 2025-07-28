/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/ScriptSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/World.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

constexpr bool g_enableScriptReloading = true;

ScriptSystem::ScriptSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
    // @FIXME: Issue with reloaded assemblies that spawn native objects having their classes change.

    if (g_enableScriptReloading)
    {
        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            g_engine->GetScriptingService()->OnScriptStateChanged.Bind([this](const ManagedScript& script)
                {
                    Threads::AssertOnThread(g_gameThread);

                    if (!(script.state & uint32(CompiledScriptState::COMPILED)))
                    {
                        return;
                    }

                    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                    {
                        if (ANSIStringView(script.assemblyPath) == ANSIStringView(scriptComponent.script.assemblyPath))
                        {
                            HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity->Id());

                            // Reload the script
                            scriptComponent.flags |= ScriptComponentFlags::RELOADING;

                            scriptComponent.script.uuid = script.uuid;
                            scriptComponent.script.state = script.state;
                            scriptComponent.script.hotReloadVersion = script.hotReloadVersion;
                            scriptComponent.script.lastModifiedTimestamp = script.lastModifiedTimestamp;

                            OnEntityRemoved(entity);

                            scriptComponent.assembly.Reset();

                            OnEntityAdded(entity->HandleFromThis());

                            scriptComponent.flags &= ~ScriptComponentFlags::RELOADING;

                            HYP_LOG(Script, Info, "ScriptSystem: Script reloaded for entity #{}", entity->Id());
                        }
                    }
                }));
    }

    if (World* world = GetWorld())
    {
        m_delegateHandlers.Add(
            NAME("OnGameStateChange"),
            world->OnGameStateChange.Bind([this](World* world, GameStateMode previousGameStateMode, GameStateMode currentGameStateMode)
                {
                    Threads::AssertOnThread(g_gameThread);

                    HandleGameStateChanged(currentGameStateMode, previousGameStateMode);
                }));
    }

    // m_delegateHandlers.Add(
    //     NAME("OnWorldChange"),
    //     OnWorldChanged.Bind([this](World* newWorld, World* previousWorld)
    //         {
    //             Threads::AssertOnThread(g_gameThread);

    //             // Remove previous OnGameStateChange handler
    //             m_delegateHandlers.Remove(NAME("OnGameStateChange"));

    //             // If we were simulating before we need to stop it
    //             if (previousWorld != nullptr && previousWorld->GetGameState().mode == GameStateMode::SIMULATING)
    //             {
    //                 CallScriptMethod("OnPlayStop");
    //             }

    //             if (newWorld != nullptr)
    //             {
    //                 // If the newly set world is simulating we need to notify the scripts
    //                 if (newWorld->GetGameState().mode == GameStateMode::SIMULATING)
    //                 {
    //                     CallScriptMethod("OnPlayStart");
    //                 }

    //                 // Add new handler for the new world's game state changing
    //                 m_delegateHandlers.Add(
    //                     NAME("OnGameStateChange"),
    //                     newWorld->OnGameStateChange.Bind([this](World* world, GameStateMode gameStateMode)
    //                         {
    //                             Threads::AssertOnThread(g_gameThread);

    //                             const GameStateMode previousGameStateMode = world->GetGameState().mode;

    //                             HandleGameStateChanged(gameStateMode, previousGameStateMode);
    //                         }));
    //             }
    //         }));
}

void ScriptSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    World* world = GetWorld();
    ScriptComponent& scriptComponent = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (scriptComponent.flags & ScriptComponentFlags::INITIALIZED)
    {
        if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
        {
            CallScriptMethod("OnPlayStart", scriptComponent);
        }

        return;
    }

    if (!scriptComponent.resource || !scriptComponent.resource->GetManagedObject() || !scriptComponent.resource->GetManagedObject()->IsValid())
    {
        FreeResource<ManagedObjectResource>(scriptComponent.resource);
        scriptComponent.resource = nullptr;

        if (!scriptComponent.assembly)
        {
            ANSIString assemblyPath(scriptComponent.script.assemblyPath);

            if (scriptComponent.script.hotReloadVersion > 0)
            {
                // @FIXME Implement FindLastIndex
                const SizeType extensionIndex = assemblyPath.FindFirstIndex(".dll");

                if (extensionIndex != ANSIString::notFound)
                {
                    assemblyPath = assemblyPath.Substr(0, extensionIndex)
                        + "." + ANSIString::ToString(scriptComponent.script.hotReloadVersion)
                        + ".dll";
                }
                else
                {
                    assemblyPath = assemblyPath
                        + "." + ANSIString::ToString(scriptComponent.script.hotReloadVersion)
                        + ".dll";
                }
            }

            if (RC<dotnet::Assembly> assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(assemblyPath.Data()))
            {
                scriptComponent.assembly = std::move(assembly);
            }
            else
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", scriptComponent.script.assemblyPath);

                return;
            }
        }

        if (RC<dotnet::Class> classPtr = scriptComponent.assembly->FindClassByName(scriptComponent.script.className))
        {
            HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", scriptComponent.script.className, scriptComponent.script.assemblyPath);

            if (!classPtr->HasParentClass("Script"))
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", scriptComponent.script.className, scriptComponent.script.assemblyPath);

                return;
            }

            dotnet::Object* object = classPtr->NewObject();
            Assert(object != nullptr);

            scriptComponent.resource = AllocateResource<ManagedObjectResource>(object, classPtr);
            scriptComponent.resource->IncRef();

            if (!(scriptComponent.flags & ScriptComponentFlags::BEFORE_INIT_CALLED))
            {
                if (dotnet::Method* beforeInitMethodPtr = classPtr->GetMethod("BeforeInit"))
                {
                    HYP_NAMED_SCOPE("Call BeforeInit() on script component");
                    HYP_LOG(Script, Debug, "Calling BeforeInit() on script component");

                    object->InvokeMethod<void>(beforeInitMethodPtr, GetWorld(), GetScene());

                    scriptComponent.flags |= ScriptComponentFlags::BEFORE_INIT_CALLED;
                }
            }

            if (!(scriptComponent.flags & ScriptComponentFlags::INIT_CALLED))
            {
                if (dotnet::Method* initMethodPtr = classPtr->GetMethod("Init"))
                {
                    HYP_NAMED_SCOPE("Call Init() on script component");
                    HYP_LOG(Script, Info, "Calling Init() on script component");

                    object->InvokeMethod<void>(initMethodPtr, entity);

                    scriptComponent.flags |= ScriptComponentFlags::INIT_CALLED;
                }
            }
        }

        if (!scriptComponent.resource || !scriptComponent.resource->GetManagedObject() || !scriptComponent.resource->GetManagedObject()->IsValid())
        {
            HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", scriptComponent.script.className, scriptComponent.script.assemblyPath);

            if (scriptComponent.resource)
            {
                scriptComponent.resource->DecRef();

                FreeResource<ManagedObjectResource>(scriptComponent.resource);
                scriptComponent.resource = nullptr;
            }

            return;
        }
    }

    scriptComponent.flags |= ScriptComponentFlags::INITIALIZED;

    // Call OnPlayStart on first init if we're currently simulating
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart", scriptComponent);
    }
}

void ScriptSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    World* world = GetWorld();

    ScriptComponent& scriptComponent = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    // If we're simulating while the script is removed, call OnPlayStop so OnPlayStart never gets double called
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStop", scriptComponent);
    }

    if (scriptComponent.resource)
    {
        if (scriptComponent.resource->GetManagedObject() && scriptComponent.resource->GetManagedObject()->IsValid())
        {
            if (dotnet::Class* classPtr = scriptComponent.resource->GetManagedObject()->GetClass())
            {
                if (classPtr->HasMethod("Destroy"))
                {
                    HYP_NAMED_SCOPE("Call Destroy() on script component");

                    scriptComponent.resource->GetManagedObject()->InvokeMethodByName<void>("Destroy");
                }
            }
        }

        scriptComponent.resource->DecRef();

        FreeResource<ManagedObjectResource>(scriptComponent.resource);
        scriptComponent.resource = nullptr;
    }

    scriptComponent.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_INIT_CALLED | ScriptComponentFlags::INIT_CALLED);
}

void ScriptSystem::Process(float delta)
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    // Only update scripts if we're in simulation mode
    if (world->GetGameState().mode != GameStateMode::SIMULATING)
    {
        return;
    }

    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        AssertDebug(scriptComponent.resource != nullptr);
        AssertDebug(scriptComponent.resource->GetManagedObject() != nullptr);

        if (dotnet::Class* classPtr = scriptComponent.resource->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* updateMethodPtr = classPtr->GetMethod("Update"))
            {
                if (updateMethodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                HYP_NAMED_SCOPE("Call Update() on script component");

                scriptComponent.resource->GetManagedObject()->InvokeMethod<void, float>(updateMethodPtr, float(delta));
            }
        }
    }
}

void ScriptSystem::HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode)
{
    HYP_SCOPE;

    if (previousGameStateMode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStop");
    }

    if (gameStateMode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart");
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName)
{
    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        AssertDebug(scriptComponent.resource != nullptr);
        AssertDebug(scriptComponent.resource->GetManagedObject() != nullptr);

        if (dotnet::Class* classPtr = scriptComponent.resource->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* methodPtr = classPtr->GetMethod(methodName))
            {
                if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                scriptComponent.resource->GetManagedObject()->InvokeMethod<void>(methodPtr);
            }
        }
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    AssertDebug(target.resource != nullptr);
    AssertDebug(target.resource->GetManagedObject() != nullptr);

    if (dotnet::Class* classPtr = target.resource->GetManagedObject()->GetClass())
    {
        if (dotnet::Method* methodPtr = classPtr->GetMethod(methodName))
        {
            if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
            {
                // Stubbed method, don't waste cycles calling it if it's not implemented
                return;
            }

            target.resource->GetManagedObject()->InvokeMethod<void>(methodPtr);
        }
    }
}

} // namespace hyperion
