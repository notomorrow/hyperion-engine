/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector2.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class View;

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

namespace EditorHelpers {

Task<void> CaptureViewToPNGAsync(
    const Handle<View>& view,
    const FilePath& path,
    const Vec2u& wireframeExtent,
    const Vec2u& outputExtent = Vec2u::Zero());

} // namespace EditorHelpers
} // namespace Hyperion
