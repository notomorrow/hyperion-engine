#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {

class Game;
class World;

// Tests that run in an editor project rather than the game: the editor's EditorGameTestCommandlet calls RunPhase on each build of
// the game it loads, finding it by name, so the phases run on whichever build is current
HYP_CLASS(NoScriptBindings)
class NativeTestEditorTests final : public ObjectBase
{
    HYP_OBJECT_BODY(NativeTestEditorTests);

public:
    // Returns the phase's test report, the same as the game's own tests write
    HYP_METHOD()
    static String RunPhase(const String& phase, const Handle<Game>& game, const Handle<World>& world);
};

} // namespace Hyperion
