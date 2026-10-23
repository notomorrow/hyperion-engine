/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>

namespace Hyperion {

class Class;
class EditorSubsystem;

// Native objects can't change class in place, so nodes of a reloaded game module's classes are rebuilt on its new build
EDITOR_API void RebuildRetiredModuleNodes(EditorSubsystem* subsystem, const Array<const Class*>& retiredClasses);

} // namespace Hyperion
