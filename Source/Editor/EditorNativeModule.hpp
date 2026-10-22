/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class Game;
class EditorSubsystem;

enum class GameProjectLanguage : uint32
{
    None = 0,
    Native,
    Managed
};

EDITOR_API String GetNativeProjectName(const FilePath& projectFilepath);

EDITOR_API FilePath GetNativeModulePath(const FilePath& projectFilepath);

EDITOR_API TResult<Handle<Game>> CreateGameFromNativeModule(const FilePath& projectFilepath);

EDITOR_API Result ReloadGameModule(EditorSubsystem* subsystem);

EDITOR_API void UpdateGameModules(EditorSubsystem* subsystem);

EDITOR_API FilePath GetManagedProjectFilePath(const FilePath& projectFilepath);

EDITOR_API bool IsManagedProject(const FilePath& projectFilepath);

EDITOR_API FilePath GetManagedModulePath(const FilePath& projectFilepath);

EDITOR_API Result WriteManagedProjectProps(const FilePath& projectFilepath);

EDITOR_API TResult<Handle<Game>> CreateGameFromManagedModule(const FilePath& projectFilepath);

EDITOR_API GameProjectLanguage GetGameProjectLanguage(const FilePath& projectFilepath);

EDITOR_API bool IsGameProjectUnmodified(const FilePath& projectFilepath);

EDITOR_API Result GenerateGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language);

EDITOR_API Result RemoveGameProjectFiles(const FilePath& projectFilepath);

EDITOR_API Result RenameGameProjectFiles(const FilePath& projectFilepath, const String& oldName, const String& newName);

} // namespace Hyperion
