/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class Entity;
class World;
class AssetRegistry;

struct ScriptComponent;
struct ScriptDesc;
struct GameState;
struct StringHash;

#ifdef HYP_STRATA
struct ScriptObjectData_Strata;

namespace Strata {

void ClearFunctionPointerCacheForModule(StringHash moduleHash);

void* ResolveFunctionPointer(ScriptObjectData_Strata* data, const char* name);

} // namespace Strata
#endif // HYP_STRATA

namespace EntityScripting {

template <class TWorld, class T>
static void QueryScriptedEntities(TWorld& world, T&& function)
{
    for (auto&& scene : world.GetScenes())
    {
        for (auto [entity, scriptComponent] : scene->GetEntityManager()->template GetEntitySet<ScriptComponent>())
        {
            function(entity, scriptComponent);
        }
    }
}

void InitializeEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState);
void ShutdownEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState);

ANSIString GetCSharpAssemblyLoadPath(const ScriptDesc& scriptDesc);

Array<Name> FindUnavailableScripts(AssetRegistry& registry);

void UpdateScriptedEntities(World& world, float delta);

} // namespace EntityScripting

} // namespace Hyperion
