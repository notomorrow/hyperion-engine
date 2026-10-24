/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>

namespace Hyperion {

class Class;
class EditorProject;
class EditorSubsystem;

// Native objects can't change class in place, so scene nodes and loaded assets of a reloaded game module's classes are rebuilt on
// its new build, and systems and subsystems of its classes are replaced in the project's world. The subsystem is null without an editor
EDITOR_API void RebuildRetiredModuleObjects(EditorProject* project, EditorSubsystem* subsystem, const Array<const Class*>& retiredClasses);

} // namespace Hyperion
