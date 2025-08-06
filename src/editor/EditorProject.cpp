/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/Scene.hpp>

#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMDeserializedObject.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static const String g_defaultProjectName = "UntitledProject";

EditorProject::EditorProject()
    : EditorProject(Name::Invalid())
{
}

EditorProject::EditorProject(Name name)
    : m_name(name),
      m_lastSavedTime(~0ull)
{
    m_actionStack = CreateObject<EditorActionStack>(WeakHandleFromThis());
}

EditorProject::~EditorProject()
{
}

void EditorProject::Init()
{
    if (m_name.IsValid())
    {
        if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to create asset package for project '{}': {}", m_name, createPackageResult.GetError().GetMessage());
        }
    }

    InitObject(m_actionStack);

    for (const Handle<Scene>& scene : m_scenes)
    {
        InitObject(scene);

        OnSceneAdded(scene);
    }

    SetReady(true);
}

void EditorProject::SetName(Name name)
{
    if (m_name == name)
    {
        return;
    }

    m_name = name;

    if (m_package.IsValid())
    {
        m_package->SetName(name);
    }
    else if (IsInitCalled())
    {
        if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to create asset package for project '{}': {}", m_name, createPackageResult.GetError().GetMessage());
        }
    }
}

Result EditorProject::CreatePackage()
{
    // only create if it isn't created yet
    if (m_package.IsValid())
    {
        return {};
    }

    Name packageName = GetName();

    if (!packageName.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Project name is not set");
    }

    Handle<AssetRegistry> assetRegistry = g_assetManager->GetAssetRegistry();

    Handle<AssetPackage> rootPackage = assetRegistry->GetPackageFromPath(*packageName, true);
    Assert(rootPackage.IsValid());

    m_package = rootPackage;

    OnPackageCreated(m_package);

    return {};
}

void EditorProject::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);

    if (IsInitCalled())
    {
        OnSceneAdded(scene);
    }
}

void EditorProject::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (!m_scenes.Contains(scene))
    {
        return;
    }

    if (IsInitCalled())
    {
        OnSceneRemoved(scene);
    }

    m_scenes.Erase(scene);
}

FilePath EditorProject::GetProjectsDirectory() const
{
    return GetResourceDirectory() / "projects";
}

bool EditorProject::IsSaved() const
{
    return uint64(m_lastSavedTime) != ~0ull;
}

void EditorProject::Close()
{
    HYP_SCOPE;
}

Result EditorProject::Save()
{
    return SaveAs(m_filepath);
}

Result EditorProject::SaveAs(FilePath filepath)
{
    HYP_SCOPE;

    if (!m_name.IsValid())
    {
        m_name = GetNextDefaultProjectName(g_defaultProjectName);

        if (!m_name.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Failed to generate a project name");
        }
    }

    if (!m_package.IsValid())
    {
        if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
        {
            return createPackageResult.GetError();
        }
    }

    if (filepath.Empty())
    {
        filepath = GetProjectsDirectory() / *m_name;
    }

    if (!filepath.Exists() && !filepath.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory");
    }

    if (!filepath.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a directory", filepath);
    }

    const Time previousLastSavedTime = m_lastSavedTime;
    m_lastSavedTime = Time::Now();

    const FilePath projectFilepath = filepath / (String(*m_name) + ".hypproj");

    FileByteWriter byteWriter(projectFilepath);
    HYP_DEFER({ byteWriter.Close(); });

    FBOMWriter writer { FBOMWriterConfig {} };
    writer.Append(*this);

    if (auto err = writer.Emit(&byteWriter))
    {
        m_lastSavedTime = previousLastSavedTime;

        return HYP_MAKE_ERROR(Error, "Failed to write project to disk");
    }

    Result result;

    {
        if (Result packageSaveResult = m_package->Save(filepath); packageSaveResult.HasError())
        {
            result = packageSaveResult;
        }
    }

    if (result)
    {
        // Update m_filepath when save was successful.
        m_filepath = filepath;

        OnProjectSaved(HandleFromThis());
    }

    return result;
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    HYP_SCOPE;

    FilePath directory;
    FilePath projectFilepath;

    if (filepath.IsDirectory())
    {
        directory = filepath;

        for (const FilePath& file : filepath.GetAllFilesInDirectory())
        {
            if (file.EndsWith(".hypproj"))
            {
                projectFilepath = file;

                break;
            }
        }
    }
    else
    {
        directory = filepath.BasePath();
        projectFilepath = filepath;
    }

    if (!directory.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Directory does not exist");
    }

    if (!projectFilepath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Project file does not exist");
    }

    FBOMObject projectObject;
    FBOMReader reader({});

    if (FBOMResult err = reader.LoadFromFile(projectFilepath, projectObject))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load project");
    }

    Optional<const Handle<EditorProject>&> projectOpt = projectObject.m_deserializedObject->TryGet<Handle<EditorProject>>();

    if (!projectOpt)
    {
        return HYP_MAKE_ERROR(Error, "Failed to get project");
    }

    Handle<EditorProject> project = *projectOpt;

    if (project->GetName().IsValid())
    {
        if (Result createPackageResult = project->CreatePackage(); createPackageResult.HasError())
        {
            return createPackageResult.GetError();
        }
    }

    Proc<TResult<Handle<AssetPackage>>(const FilePath& directory)> initializePackage;

    initializePackage = [&initializePackage](const FilePath& directory) -> TResult<Handle<AssetPackage>>
    {
        HYP_NAMED_SCOPE_FMT("Initialize package %s", *directory);

        Handle<AssetPackage> package = CreateObject<AssetPackage>();

        package->SetName(CreateNameFromDynamicString(directory.Basename()));

        AssetPackageSet subpackages;

        for (const FilePath& subdirectory : directory.GetSubdirectories())
        {
            TResult<Handle<AssetPackage>> subpackageResult = initializePackage(subdirectory);

            if (subpackageResult.HasError())
            {
                return subpackageResult;
            }

            subpackages.Insert(std::move(subpackageResult.GetValue()));
        }

        // @TODO Load assets from files in directory

        package->SetSubpackages(std::move(subpackages));

        return package;
    };

    AssetPackageSet packages;

    for (const FilePath& subdirectory : directory.GetSubdirectories())
    {
        TResult<Handle<AssetPackage>> packageResult = initializePackage(subdirectory);

        if (packageResult.HasError())
        {
            return packageResult.GetError();
        }

        packages.Insert(std::move(packageResult.GetValue()));
    }

    project->m_package->SetSubpackages(std::move(packages));

    return project;
}

Name EditorProject::GetNextDefaultProjectName_Impl(const String& defaultProjectName) const
{
    return Name::Invalid();
}

} // namespace hyperion
