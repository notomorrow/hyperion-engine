#include <core/utilities/GlobalContext.hpp>

namespace hyperion {
namespace utilities {

#pragma region GlobalContextRegistry

thread_local GlobalContextRegistry* g_globalContextRegistry = nullptr;

HYP_API GlobalContextRegistry* GetGlobalContextRegistryForCurrentThread()
{
    if (!g_globalContextRegistry)
    {
        g_globalContextRegistry = new GlobalContextRegistry();
    }

    return g_globalContextRegistry;
}

GlobalContextRegistry::GlobalContextRegistry()
    : m_ownerThreadId(ThreadId::Current())
{
}

GlobalContextRegistry::~GlobalContextRegistry() = default;

#pragma endregion GlobalContextRegistry

} // namespace utilities
} // namespace hyperion