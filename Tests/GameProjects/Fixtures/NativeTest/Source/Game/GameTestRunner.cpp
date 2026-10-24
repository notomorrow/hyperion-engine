#include <Game/GameTestRunner.hpp>

#include <Framework/Game.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Scene/World.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Logging/Logger.hpp>

#include <chrono>
#include <cstdlib>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Game);

namespace tests {

GameTestContext::GameTestContext(Game* game, const Handle<AssetRegistry>& assetRegistry, const String& phase, const Handle<World>& world)
    : m_game(game),
      m_assetRegistry(assetRegistry),
      m_contentDirectory(assetRegistry.IsValid() ? assetRegistry->GetRootPath() : FilePath()),
      m_phase(phase),
      m_world(world)
{
}

bool GameTestContext::Check(bool condition, const String& message)
{
    if (!condition && m_failureMessage.Empty())
    {
        m_failureMessage = message;
    }

    return condition;
}

String RunGameTests(GameTestContext& context, const Array<GameTestDefinition>& tests)
{
    JSON::JArray resultsArray;
    uint32 numFailed = 0;

    if (!context.GetAssetRegistry().IsValid())
    {
        ++numFailed;

        resultsArray.PushBack(JSON::Object {
            { "name", JSON::Value(String("Setup")) },
            { "passed", JSON::Value(false) },
            { "message", JSON::Value(String("Game has no AssetRegistry")) },
            { "durationMs", JSON::Value(0.0) } });
    }
    else
    {
        for (const GameTestDefinition& test : tests)
        {
            if (context.GetPhase() != test.phase)
            {
                continue;
            }

            context.Reset();

            const auto startTime = std::chrono::steady_clock::now();

            test.function(context);

            const double durationMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();

            if (context.HasFailed())
            {
                ++numFailed;

                HYP_LOG(Game, Error, "[FAIL] {}: {}", test.name, context.GetFailureMessage());
            }
            else
            {
                HYP_LOG(Game, Info, "[PASS] {}", test.name);
            }

            resultsArray.PushBack(JSON::Object {
                { "name", JSON::Value(String(test.name)) },
                { "passed", JSON::Value(!context.HasFailed()) },
                { "message", context.HasFailed() ? JSON::Value(context.GetFailureMessage()) : JSON::Value(JSON::JSNull()) },
                { "durationMs", JSON::Value(durationMs) } });
        }
    }

    HYP_LOG(Game, Info, "Game tests ({}): {} passed, {} failed", context.GetPhase(), resultsArray.Size() - numFailed, numFailed);

    JSON::Object report {
        { "phase", JSON::Value(context.GetPhase()) },
        { "tests", JSON::Value(std::move(resultsArray)) } };

    return JSON::Value(std::move(report)).ToString(true);
}

void RunGameTestsFromEnvironment(Game* game, const Array<GameTestDefinition>& tests)
{
    const char* phaseVariable = std::getenv("HYP_GAME_TEST_PHASE");
    const char* resultsVariable = std::getenv("HYP_GAME_TEST_RESULTS");

    const String phase = phaseVariable != nullptr && *phaseVariable != '\0' ? String(phaseVariable) : String(g_writePhase);

    GameTestContext context(game, game->GetAssetRegistry(), phase);

    const String reportText = RunGameTests(context, tests);

    if (resultsVariable == nullptr || *resultsVariable == '\0')
    {
        return;
    }

    FileByteWriter writer { FilePath(resultsVariable) };

    if (!writer.IsOpen())
    {
        HYP_LOG(Game, Error, "Failed to open {} for writing", resultsVariable);

        return;
    }

    writer.Write(reportText.Data(), reportText.Size());
    writer.Close();
}

} // namespace tests
} // namespace Hyperion
