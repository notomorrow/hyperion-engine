/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

static constexpr const char* g_defaultProjectContentSubfolder = "Content";

ENGINE_API FilePath FindProjectFile(const FilePath& projectDirectory);

// Reads and normalizes the project's ContentSubfolder field
ENGINE_API TResult<String> ReadProjectContentSubfolder(const FilePath& projectFile);
ENGINE_API TResult<String> NormalizeProjectContentSubfolder(const String& subfolder);

/// projectPath may be the project directory or its .hypproject file.
/// Projects without a ContentSubfolder resolve to the project directory.
ENGINE_API TResult<FilePath> ResolveProjectContentDirectory(const FilePath& projectPath);

} // namespace Hyperion
