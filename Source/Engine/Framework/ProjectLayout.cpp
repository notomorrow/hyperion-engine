/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/ProjectLayout.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Core/Reflection/BoxedValue.hpp>

namespace Hyperion {

FilePath FindProjectFile(const FilePath& projectDirectory)
{
    for (const FilePath& file : projectDirectory.GetAllFilesInDirectory())
    {
        if (file.EndsWith(".hypproject"))
        {
            return file;
        }
    }

    return FilePath();
}

TResult<String> ReadProjectContentSubfolder(const FilePath& projectFile)
{
    TResult<BoxedValue> fieldResult = FieldFromHMF(projectFile, "ContentSubfolder"_sh);

    if (fieldResult.HasError())
    {
        return fieldResult.GetError();
    }

    const BoxedValue& value = fieldResult.GetValue();

    if (!value.Is<String>())
    {
        return HYP_MAKE_ERROR(Error, "ContentSubfolder of project {} is not a string", projectFile);
    }

    return NormalizeProjectContentSubfolder(value.Get<String>());
}

TResult<String> NormalizeProjectContentSubfolder(const String& subfolder)
{
    const String trimmed = subfolder.Trimmed().ReplaceAll("\\", "/");

    if (trimmed.Empty())
    {
        return String::empty;
    }

    if (FilePath(trimmed).IsAbsolute() || trimmed.Contains(':'))
    {
        return HYP_MAKE_ERROR(Error, "Content subfolder '{}' must be relative to the project directory", subfolder);
    }

    String normalized;

    for (const String& segment : trimmed.Split('/'))
    {
        if (segment.Empty())
        {
            continue;
        }

        if (segment == "." || segment == "..")
        {
            return HYP_MAKE_ERROR(Error, "Content subfolder '{}' must stay inside the project directory", subfolder);
        }

        if (normalized.Any())
        {
            normalized += "/";
        }

        normalized += segment;
    }

    return normalized;
}

TResult<FilePath> ResolveProjectContentDirectory(const FilePath& projectPath)
{
    FilePath projectDirectory;
    FilePath projectFile;

    if (projectPath.IsDirectory())
    {
        projectDirectory = projectPath;
        projectFile = FindProjectFile(projectPath);
    }
    else
    {
        projectDirectory = projectPath.BasePath();
        projectFile = projectPath;
    }

    if (projectFile.Empty() || !projectFile.Exists())
    {
        return projectDirectory;
    }

    TResult<String> subfolderResult = ReadProjectContentSubfolder(projectFile);

    if (subfolderResult.HasError())
    {
        return subfolderResult.GetError();
    }

    if (subfolderResult.GetValue().Empty())
    {
        return projectDirectory;
    }

    return projectDirectory / subfolderResult.GetValue();
}

} // namespace Hyperion
