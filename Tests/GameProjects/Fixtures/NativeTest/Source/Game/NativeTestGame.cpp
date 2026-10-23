#include <Game/NativeTestGame.hpp>
#include <Game/NativeTestBuild.hpp>
#include <Game/NativeTests.hpp>

#include <HyperionEngine.hpp>

#include <Core/Logging/Logger.hpp>

#include <NativeTestGame.generated.inl>

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

} // namespace Hyperion

extern "C"
{

HYP_EXPORT const char* HypGameModule_GetGameClassName()
{
    return Hyperion::NativeTestGameClassName;
}

HYP_EXPORT void HypGameModule_GetEngineVersion(Hyperion::uint32* outMajor, Hyperion::uint32* outMinor, Hyperion::uint32* outPatch)
{
    *outMajor = HYP_VERSION_MAJOR;
    *outMinor = HYP_VERSION_MINOR;
    *outPatch = HYP_VERSION_PATCH;
}

}
