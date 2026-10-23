#include <Game/NativeTestGame.hpp>
#include <Game/NativeTests.hpp>

#include <HyperionEngine.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Game);

NativeTestGame::NativeTestGame()
    : Game()
{
}

NativeTestGame::~NativeTestGame()
{
}

// The content has no World, so the base class stops here without launching; the asset registry is up, which is all the tests need
void NativeTestGame::AfterContentLoaded()
{
    Game::AfterContentLoaded();

    HYP_LOG(Game, Info, "NativeTestGame content loaded, running tests");

    tests::RunGameTestsFromEnvironment(this, tests::GetNativeGameTests());

    Hyp_RequestQuit();
}

#pragma region Reflection

const Class* g_clsNativeTestGame = nullptr;

const Class* NativeTestGame::StaticClass()
{
    return g_clsNativeTestGame;
}

// clang-format off

HYP_BEGIN_CLASS(NativeTestGame, -1, 0, NAME("Game"))
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(NativeTestGame);

#pragma endregion Reflection

} // namespace Hyperion

extern "C"
{

HYP_EXPORT const char* HypGameModule_GetGameClassName()
{
    return "NativeTestGame";
}

HYP_EXPORT void HypGameModule_GetEngineVersion(Hyperion::uint32* outMajor, Hyperion::uint32* outMinor, Hyperion::uint32* outPatch)
{
    *outMajor = HYP_VERSION_MAJOR;
    *outMinor = HYP_VERSION_MINOR;
    *outPatch = HYP_VERSION_PATCH;
}

}
