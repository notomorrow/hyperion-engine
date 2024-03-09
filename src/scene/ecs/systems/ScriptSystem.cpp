#include <scene/ecs/systems/ScriptSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/DotNetSystem.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ScriptSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    ScriptComponent &script_component = entity_manager.GetComponent<ScriptComponent>(entity);

    script_component.assembly = nullptr;
    script_component.object = nullptr;

    if (RC<dotnet::Assembly> managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(script_component.script_info.assembly_name.Data())) {
        if (dotnet::Class *class_ptr = managed_assembly->GetClassObjectHolder().FindClassByName(script_component.script_info.class_name.Data())) {
            script_component.object = class_ptr->NewObject();

            if (class_ptr->HasMethod("BeforeInit")) {
                script_component.object->InvokeMethod<void, ManagedHandle>(
                    "BeforeInit",
                    CreateManagedHandleFromID(entity_manager.GetScene()->GetID())
                );
            }

            if (class_ptr->HasMethod("Init")) {
                script_component.object->InvokeMethod<void, ManagedEntity>(
                    "Init",
                    ManagedEntity { entity.Value() }
                );
            }
        }

        script_component.assembly = std::move(managed_assembly);
    }

    if (!script_component.assembly) {
        DebugLog(LogType::Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '%s'\n", script_component.script_info.assembly_name.Data());
    }

    if (!script_component.object) {
        DebugLog(LogType::Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '%s' from assembly '%s'\n", script_component.script_info.class_name.Data(), script_component.script_info.assembly_name.Data());
    }
}

void ScriptSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    ScriptComponent &script_component = entity_manager.GetComponent<ScriptComponent>(entity);

    if (script_component.object != nullptr) {
        if (dotnet::Class *class_ptr = script_component.object->GetClass()) {
            if (class_ptr->HasMethod("Destroy")) {
                script_component.object->InvokeMethod<void>("Destroy");
            }
        }
    }

    script_component.object = nullptr;
    script_component.assembly = nullptr;
}

void ScriptSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, script_component] : entity_manager.GetEntitySet<ScriptComponent>()) {
        if (!script_component.object) {
            continue;
        }

        if (dotnet::Class *class_ptr = script_component.object->GetClass()) {
            if (class_ptr->HasMethod("Update")) {
                script_component.object->InvokeMethod<void, float>("Update", float(delta));
            }
        }
    }
}

} // namespace hyperion::v2