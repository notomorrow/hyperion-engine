/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Name/Name.hpp>

#include <Asset/AssetPath.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class AssetRegistry;

HYP_CLASS()
class EDITOR_API EditorAssetFolders final : public ObjectBase
{
    HYP_OBJECT_BODY(EditorAssetFolders);

public:
    static constexpr const char* s_fileName = "AssetFolders.txt";

    EditorAssetFolders();

    EditorAssetFolders(const EditorAssetFolders& other) = delete;
    EditorAssetFolders& operator=(const EditorAssetFolders& other) = delete;

    virtual ~EditorAssetFolders() override;

    static bool IsValidFolderName(const String& name);
    static bool IsValidFolderPath(const String& path);

    static String NormalizeFolderPath(const String& path);
    static String JoinFolderPath(const String& parentPath, const String& name);
    static String GetParentFolderPath(const String& path);
    static String GetFolderName(const String& path);
    static bool IsSameOrDescendantFolder(const String& path, const String& ancestorPath);

    HYP_METHOD()
    bool HasFolder(const String& path) const;

    Array<String> GetAllFolders() const;

    HYP_METHOD()
    Array<Name> GetFolderPaths() const;

    Array<String> GetSubfolders(const String& path) const;

    Array<AssetPath> GetAssetsInFolder(const String& path, bool recursive) const;

    HYP_METHOD()
    uint32 CountAssetsInFolder(const String& path, bool recursive) const;

    String GetAssetFolder(const AssetPath& assetPath) const;
    bool HasAssetFolder(const AssetPath& assetPath) const;

    HYP_METHOD()
    String GetAssetFolderOf(uint32 bucketIndex, Name assetName) const;

    HYP_METHOD()
    String MakeUniqueFolderName(const String& parentPath, const String& name) const;

    HYP_METHOD()
    bool CreateFolder(const String& path);

    /// Removes the folder, its subfolders and the folder entries of the assets in them
    HYP_METHOD()
    bool RemoveFolder(const String& path);

    /// Returns the folder's new path, or an empty string if it could not be moved
    HYP_METHOD()
    String MoveFolder(const String& path, const String& newParentPath);

    HYP_METHOD()
    String RenameFolder(const String& path, const String& newName);

    void SetAssetFolder(const AssetPath& assetPath, const String& folderPath);
    void SetAssetFolder(Span<const AssetPath> assetPaths, const String& folderPath);

    HYP_METHOD()
    void SetAssetFolderOf(uint32 bucketIndex, Name assetName, const String& folderPath);

    void RemoveAssetFolder(const AssetPath& assetPath);

    /// Gives every registered asset without a folder the folder named after its bucket. Returns true if any were added.
    HYP_METHOD()
    bool SyncWithRegistry();

    void PruneMissingAssets(const AssetRegistry& registry);

    void BindRegistry(const Handle<AssetRegistry>& registry);

    Result Load(const FilePath& filepath);
    Result Save(const FilePath& filepath);

    HYP_METHOD()
    bool IsDirty() const;

    HYP_FIELD()
    ScriptableDelegate<void> OnFoldersChanged;

private:
    void NotifyChanged();

    bool CreateFolder_Internal(const String& normalizedPath);
    void MoveFolderEntries_Internal(const String& fromPath, const String& toPath);
    bool HasChildNamed_Internal(const String& parentPath, const String& name) const;

    mutable Mutex m_mutex;

    Set<String> m_folders;
    Map<AssetPath, String> m_assetFolders;

    WeakHandle<AssetRegistry> m_registry;
    DelegateHandler m_onAssetRenamed;

    bool m_isDirty = false;
    bool m_hasUnsavedDefaults = false;
};

} // namespace Hyperion
