/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorProject.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <asset/Assets.hpp>
#include <asset/serialization/fbom/FBOMWriter.hpp>
#include <asset/serialization/fbom/FBOMReader.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

namespace hyperion {

EditorProject::EditorProject()
    : m_last_saved_time(~0ull)
{
    AssetManager::GetInstance()->ForEachAssetCollector([this](const Handle<AssetCollector> &asset_collector)
    {
        m_non_owned_asset_collectors.PushBack(asset_collector);
    });

    Handle<Camera> camera = CreateObject<Camera>();
    camera->SetFlags(CameraFlags::MATCH_WINDOW_SIZE);
    camera->SetFOV(70.0f);
    camera->SetNear(0.01f);
    camera->SetFar(30000.0f);

    m_scene = CreateObject<Scene>(nullptr, camera);
}

EditorProject::~EditorProject()
{
    for (const Handle<AssetCollector> &asset_collector : m_owned_asset_collectors) {
        AssetManager::GetInstance()->RemoveAssetCollector(asset_collector);
    }
}

void EditorProject::SetAssetCollectors(const Array<Handle<AssetCollector>> &asset_collectors)
{
    HYP_SCOPE;

    for (const Handle<AssetCollector> &asset_collector : m_owned_asset_collectors) {
        AssetManager::GetInstance()->RemoveAssetCollector(asset_collector);
    }

    m_owned_asset_collectors = asset_collectors;

    for (const Handle<AssetCollector> &asset_collector : m_owned_asset_collectors) {
        AssetManager::GetInstance()->AddAssetCollector(asset_collector);
    }
}

bool EditorProject::IsSaved() const
{
    return uint64(m_last_saved_time) != ~0ull;
}

void EditorProject::Close()
{
    HYP_SCOPE;
}

Result<void> EditorProject::Save()
{
    if (m_filepath.Any()) {
        return Save(m_filepath);
    }

    return Save(AssetManager::GetInstance()->GetBasePath() / "projects" / m_uuid.ToString() + ".hyp");
}

Result<void> EditorProject::Save(const FilePath &filepath)
{
    HYP_SCOPE;

    if (filepath.Any()) {
        m_filepath = filepath;
    }

    if (m_filepath.Empty()) {
        return HYP_MAKE_ERROR(Error, "No filepath set");
    }

    const Time previous_last_saved_time = m_last_saved_time;
    m_last_saved_time = Time::Now();

    FileByteWriter byte_writer(m_filepath);

    HYP_DEFER({
        byte_writer.Close();
    });

    fbom::FBOMWriter writer { fbom::FBOMWriterConfig { } };
    writer.Append(*this);

    if (auto err = writer.Emit(&byte_writer)) {
        m_last_saved_time = previous_last_saved_time;

        return HYP_MAKE_ERROR(Error, "Failed to write project to disk");
    }

    return { };
}

} // namespace hyperion
