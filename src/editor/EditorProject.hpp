/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_PROJECT_HPP
#define HYPERION_EDITOR_PROJECT_HPP

#include <core/Handle.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>
#include <core/object/HypClassUtils.hpp>

#include <core/utilities/UUID.hpp>
#include <core/utilities/Result.hpp>

#include <core/system/Time.hpp>

namespace hyperion {

class Scene;
class AssetCollector;
class AssetRegistry;

HYP_CLASS()
class HYP_API EditorProject : public EnableRefCountedPtrFromThis<EditorProject>
{
    HYP_OBJECT_BODY(EditorProject);

public:
    EditorProject();
    ~EditorProject();

    HYP_METHOD(Property="UUID", Serialize=true)
    HYP_FORCE_INLINE const UUID &GetUUID() const
        { return m_uuid; }

    /*! \internal For serialization only. */
    HYP_METHOD(Property="UUID", Serialize=true)
    HYP_FORCE_INLINE void SetUUID(const UUID &uuid)
        { m_uuid = uuid; }

    HYP_METHOD(Property="Scene", Serialize=true)
    HYP_FORCE_INLINE const Handle<Scene> &GetScene() const
        { return m_scene; }

    /*! \internal For serialization only. */
    HYP_METHOD(Property="Scene", Serialize=true)
    HYP_FORCE_INLINE void SetScene(const Handle<Scene> &scene)
        { m_scene = scene; }

    HYP_METHOD(Property="LastSavedTime", Serialize=true)
    HYP_FORCE_INLINE Time GetLastSavedTime() const
        { return m_last_saved_time; }

    /*! \internal For serialization only. */
    HYP_METHOD(Property="LastSavedTime", Serialize=true)
    HYP_FORCE_INLINE void SetLastSavedTime(Time last_saved_time)
        { m_last_saved_time = last_saved_time; }

    HYP_METHOD(Property="AssetRegistry", Serialize=true)
    HYP_FORCE_INLINE const Handle<AssetRegistry> &GetAssetRegistry() const
        { return m_asset_registry; }

    /*! \internal Serialization only */
    HYP_METHOD(Property="AssetRegistry", Serialize=true)
    HYP_FORCE_INLINE void SetAssetRegistry(const Handle<AssetRegistry> &asset_registry)
        { m_asset_registry = asset_registry; }

    HYP_METHOD()
    bool IsSaved() const;

    Result<void> Save();
    Result<void> Save(const FilePath &filepath);

    HYP_METHOD()
    void Close();

private:
    UUID                            m_uuid;
    Time                            m_last_saved_time;

    FilePath                        m_filepath;

    Handle<Scene>                   m_scene;

    Handle<AssetRegistry>           m_asset_registry;
};

} // namespace hyperion

#endif
