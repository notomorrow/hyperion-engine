#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Game;
class World;
class AssetRegistry;

namespace tests {

// the first launch writes the content the second launch reads back
static constexpr const char* g_writePhase = "write";
static constexpr const char* g_readPhase = "read";

// a packaged build of the game runs this phase on what it was cooked with
static constexpr const char* g_cookedPhase = "cooked";

class GameTestContext
{
public:
    GameTestContext(Game* game, const Handle<AssetRegistry>& assetRegistry, const String& phase, const Handle<World>& world = Handle<World>());

    Game* GetGame() const
    {
        return m_game;
    }

    const Handle<AssetRegistry>& GetAssetRegistry() const
    {
        return m_assetRegistry;
    }

    const FilePath& GetContentDirectory() const
    {
        return m_contentDirectory;
    }

    const String& GetPhase() const
    {
        return m_phase;
    }

    // the World being edited, for the reload tests; the other tests have none
    const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    // Records the first failure of the running test; returns the condition so the test can bail out
    bool Check(bool condition, const String& message);

    bool HasFailed() const
    {
        return m_failureMessage.Any();
    }

    const String& GetFailureMessage() const
    {
        return m_failureMessage;
    }

    void Reset()
    {
        m_failureMessage = String::empty;
    }

private:
    Game* m_game;
    Handle<AssetRegistry> m_assetRegistry;
    FilePath m_contentDirectory;
    String m_phase;
    Handle<World> m_world;
    String m_failureMessage;
};

using GameTestFunction = void (*)(GameTestContext& context);

struct GameTestDefinition
{
    const char* name;
    const char* phase;
    GameTestFunction function;
};

// Runs the tests of the context's phase, returning the report the harness (Tests/GameProjects) reads
String RunGameTests(GameTestContext& context, const Array<GameTestDefinition>& tests);

// Runs the tests for the phase given by HYP_GAME_TEST_PHASE, writing a report to HYP_GAME_TEST_RESULTS
// for the harness (Tests/GameProjects) to read
void RunGameTestsFromEnvironment(Game* game, const Array<GameTestDefinition>& tests);

} // namespace tests
} // namespace Hyperion
