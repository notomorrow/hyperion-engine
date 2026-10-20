/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorAssetFolders.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/AssetTypes.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Utilities/Format.hpp>

#include <EditorAssetFolders.generated.inl>

#include <algorithm>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

static constexpr char DisallowedFolderNameChars[] = { '/', '\\', '|', '\t', '\r', '\n' };

static Array<String> SplitFolderPath(const String& path)
{
    Array<String> segments;

    auto split = path.Split('/', '\\');
    segments.Reserve(split.Size());

    for (const String& segment : split)
    {
        if (String trimmed = segment.Trimmed(); trimmed.Any())
        {
            segments.PushBack(std::move(trimmed));
        }
    }

    return segments;
}

/// path is a normalized path at or below fromPath
static String ReplaceFolderPrefix(const String& path, const String& fromPath, const String& toPath)
{
    if (path.Length() == fromPath.Length())
    {
        return toPath;
    }

    return EditorAssetFolders::JoinFolderPath(toPath, String(path.Substr(fromPath.Any() ? fromPath.Length() + 1 : 0)));
}

static String MakeAssetKey(const AssetPath& assetPath)
{
    return String(GetAssetBucketName(assetPath.bucketIndex)) + "/" + *assetPath.assetName;
}

EditorAssetFolders::EditorAssetFolders()
{
}

EditorAssetFolders::~EditorAssetFolders()
{
    m_onAssetRenamed.Reset();
}

bool EditorAssetFolders::IsValidFolderName(const String& name)
{
    if (name.Trimmed().Empty())
    {
        return false;
    }

    for (char disallowed : DisallowedFolderNameChars)
    {
        if (name.Contains(disallowed))
        {
            return false;
        }
    }

    return true;
}

bool EditorAssetFolders::IsValidFolderPath(const String& path)
{
    for (const String& segment : SplitFolderPath(path))
    {
        if (!IsValidFolderName(segment))
        {
            return false;
        }
    }

    return true;
}

String EditorAssetFolders::NormalizeFolderPath(const String& path)
{
    return String::Join(SplitFolderPath(path), '/');
}

String EditorAssetFolders::JoinFolderPath(const String& parentPath, const String& name)
{
    return NormalizeFolderPath(parentPath + "/" + name);
}

String EditorAssetFolders::GetParentFolderPath(const String& path)
{
    const String normalized = NormalizeFolderPath(path);
    const size_t separatorIndex = normalized.FindLastIndex('/');

    return separatorIndex == String::NotFound ? String::empty : String(normalized.Substr(0, separatorIndex));
}

String EditorAssetFolders::GetFolderName(const String& path)
{
    const String normalized = NormalizeFolderPath(path);
    const size_t separatorIndex = normalized.FindLastIndex('/');

    return separatorIndex == String::NotFound
        ? normalized
        : String(normalized.Substr(separatorIndex + 1));
}

bool EditorAssetFolders::IsSameOrDescendantFolder(const String& path, const String& ancestorPath)
{
    const String normalizedPath = NormalizeFolderPath(path);
    const String normalizedAncestor = NormalizeFolderPath(ancestorPath);

    if (normalizedAncestor.Empty())
    {
        return true;
    }

    if (normalizedPath == normalizedAncestor)
    {
        return true;
    }

    return normalizedPath.StartsWith(normalizedAncestor + "/");
}

bool EditorAssetFolders::HasFolder(const String& path) const
{
    const String normalized = NormalizeFolderPath(path);

    if (normalized.Empty())
    {
        return true;
    }

    Mutex::Guard guard(m_mutex);

    return m_folders.Contains(normalized);
}

Array<String> EditorAssetFolders::GetAllFolders() const
{
    Array<String> folders;

    {
        Mutex::Guard guard(m_mutex);

        for (const String& folder : m_folders)
        {
            folders.PushBack(folder);
        }
    }

    std::sort(folders.Begin(), folders.End());

    return folders;
}

Array<String> EditorAssetFolders::GetSubfolders(const String& path) const
{
    const String normalized = NormalizeFolderPath(path);

    Array<String> subfolders;

    {
        Mutex::Guard guard(m_mutex);

        for (const String& folder : m_folders)
        {
            if (GetParentFolderPath(folder) == normalized)
            {
                subfolders.PushBack(folder);
            }
        }
    }

    std::sort(subfolders.Begin(), subfolders.End(), [](const String& a, const String& b)
        {
            return a.ToLower() < b.ToLower();
        });

    return subfolders;
}

Array<Name> EditorAssetFolders::GetFolderPaths() const
{
    Array<Name> folderPaths;

    for (const String& folder : GetAllFolders())
    {
        folderPaths.PushBack(CreateNameFromDynamicString(ANSIStringView(*folder)));
    }

    return folderPaths;
}

Array<AssetPath> EditorAssetFolders::GetAssetsInFolder(const String& path, bool recursive) const
{
    const String normalized = NormalizeFolderPath(path);

    Array<AssetPath> assets;

    Mutex::Guard guard(m_mutex);

    for (const auto& it : m_assetFolders)
    {
        const bool matches = recursive
            ? IsSameOrDescendantFolder(it.second, normalized)
            : it.second == normalized;

        if (matches)
        {
            assets.PushBack(it.first);
        }
    }

    return assets;
}

uint32 EditorAssetFolders::CountAssetsInFolder(const String& path, bool recursive) const
{
    const String normalized = NormalizeFolderPath(path);

    uint32 count = 0;

    Mutex::Guard guard(m_mutex);

    for (const auto& it : m_assetFolders)
    {
        if (recursive ? IsSameOrDescendantFolder(it.second, normalized) : it.second == normalized)
        {
            ++count;
        }
    }

    return count;
}

String EditorAssetFolders::GetAssetFolder(const AssetPath& assetPath) const
{
    Mutex::Guard guard(m_mutex);

    if (const auto* entry = m_assetFolders.TryGet(assetPath))
    {
        return entry->second;
    }

    return String::empty;
}

bool EditorAssetFolders::HasAssetFolder(const AssetPath& assetPath) const
{
    Mutex::Guard guard(m_mutex);

    return m_assetFolders.Contains(assetPath);
}

String EditorAssetFolders::GetAssetFolderOf(uint32 bucketIndex, Name assetName) const
{
    if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
    {
        return String::empty;
    }

    return GetAssetFolder(AssetPath(*AssetBuckets::AllBuckets[bucketIndex], assetName));
}

bool EditorAssetFolders::HasChildNamed_Internal(const String& parentPath, const String& name) const
{
    const String lowerName = name.ToLower();

    for (const String& folder : m_folders)
    {
        if (GetParentFolderPath(folder) == parentPath && GetFolderName(folder).ToLower() == lowerName)
        {
            return true;
        }
    }

    return false;
}

String EditorAssetFolders::MakeUniqueFolderName(const String& parentPath, const String& name) const
{
    const String normalizedParent = NormalizeFolderPath(parentPath);
    const String baseName = GetFolderName(name).Any() ? GetFolderName(name) : String("New Folder");

    Mutex::Guard guard(m_mutex);

    if (!HasChildNamed_Internal(normalizedParent, baseName))
    {
        return baseName;
    }

    for (uint32 counter = 1;; ++counter)
    {
        const String candidate = HYP_FORMAT("{}_{}", baseName, counter);

        if (!HasChildNamed_Internal(normalizedParent, candidate))
        {
            return candidate;
        }
    }
}

bool EditorAssetFolders::CreateFolder_Internal(const String& normalizedPath)
{
    const Array<String> segments = SplitFolderPath(normalizedPath);

    bool created = false;
    String currentPath;

    for (const String& segment : segments)
    {
        const String lowerSegment = segment.ToLower();
        String existingPath;

        for (const String& folder : m_folders)
        {
            if (GetParentFolderPath(folder) == currentPath && GetFolderName(folder).ToLower() == lowerSegment)
            {
                existingPath = folder;
                break;
            }
        }

        if (existingPath.Any())
        {
            currentPath = existingPath;

            continue;
        }

        currentPath = JoinFolderPath(currentPath, segment);
        m_folders.Insert(currentPath);

        created = true;
    }

    return created;
}

bool EditorAssetFolders::CreateFolder(const String& path)
{
    const String normalized = NormalizeFolderPath(path);

    if (normalized.Empty() || !IsValidFolderPath(normalized))
    {
        return false;
    }

    bool created = false;

    {
        Mutex::Guard guard(m_mutex);

        created = CreateFolder_Internal(normalized);
    }

    if (created)
    {
        NotifyChanged();
    }

    return created;
}

bool EditorAssetFolders::RemoveFolder(const String& path)
{
    const String normalized = NormalizeFolderPath(path);

    if (normalized.Empty())
    {
        return false;
    }

    {
        Mutex::Guard guard(m_mutex);

        if (!m_folders.Contains(normalized))
        {
            return false;
        }

        Array<String> foldersToRemove;

        for (const String& folder : m_folders)
        {
            if (IsSameOrDescendantFolder(folder, normalized))
            {
                foldersToRemove.PushBack(folder);
            }
        }

        for (const String& folder : foldersToRemove)
        {
            m_folders.Erase(folder);
        }

        Array<AssetPath> assetsToRemove;

        for (const auto& it : m_assetFolders)
        {
            if (IsSameOrDescendantFolder(it.second, normalized))
            {
                assetsToRemove.PushBack(it.first);
            }
        }

        for (const AssetPath& assetPath : assetsToRemove)
        {
            m_assetFolders.Erase(assetPath);
        }
    }

    NotifyChanged();

    return true;
}

void EditorAssetFolders::MoveFolderEntries_Internal(const String& fromPath, const String& toPath)
{
    Array<String> movedFolders;

    for (const String& folder : m_folders)
    {
        if (IsSameOrDescendantFolder(folder, fromPath))
        {
            movedFolders.PushBack(folder);
        }
    }

    for (const String& folder : movedFolders)
    {
        m_folders.Erase(folder);
    }

    for (const String& folder : movedFolders)
    {
        m_folders.Insert(ReplaceFolderPrefix(folder, fromPath, toPath));
    }

    CreateFolder_Internal(toPath);

    for (auto& it : m_assetFolders)
    {
        if (IsSameOrDescendantFolder(it.second, fromPath))
        {
            it.second = ReplaceFolderPrefix(it.second, fromPath, toPath);
        }
    }
}

String EditorAssetFolders::MoveFolder(const String& path, const String& newParentPath)
{
    const String normalized = NormalizeFolderPath(path);
    const String normalizedParent = NormalizeFolderPath(newParentPath);

    if (normalized.Empty() || IsSameOrDescendantFolder(normalizedParent, normalized))
    {
        return String::empty;
    }

    if (GetParentFolderPath(normalized) == normalizedParent)
    {
        return normalized;
    }

    if (!HasFolder(normalized))
    {
        return String::empty;
    }

    const String newName = MakeUniqueFolderName(normalizedParent, GetFolderName(normalized));
    const String newPath = JoinFolderPath(normalizedParent, newName);

    {
        Mutex::Guard guard(m_mutex);

        if (normalizedParent.Any())
        {
            CreateFolder_Internal(normalizedParent);
        }

        MoveFolderEntries_Internal(normalized, newPath);
    }

    NotifyChanged();

    return newPath;
}

String EditorAssetFolders::RenameFolder(const String& path, const String& newName)
{
    const String normalized = NormalizeFolderPath(path);
    const String normalizedName = newName.Trimmed();

    if (normalized.Empty() || !IsValidFolderName(normalizedName))
    {
        return String::empty;
    }

    if (!HasFolder(normalized))
    {
        return String::empty;
    }

    const String currentName = GetFolderName(normalized);

    if (currentName == normalizedName)
    {
        return normalized;
    }

    const String parentPath = GetParentFolderPath(normalized);

    // a change of case only is still a rename of the same folder
    const String uniqueName = normalizedName.ToLower() == currentName.ToLower()
        ? normalizedName
        : MakeUniqueFolderName(parentPath, normalizedName);

    const String newPath = JoinFolderPath(parentPath, uniqueName);

    {
        Mutex::Guard guard(m_mutex);

        MoveFolderEntries_Internal(normalized, newPath);
    }

    NotifyChanged();

    return newPath;
}

void EditorAssetFolders::SetAssetFolder(const AssetPath& assetPath, const String& folderPath)
{
    SetAssetFolder(Span<const AssetPath>(&assetPath, size_t(1)), folderPath);
}

void EditorAssetFolders::SetAssetFolder(Span<const AssetPath> assetPaths, const String& folderPath)
{
    const String normalized = NormalizeFolderPath(folderPath);

    if (!IsValidFolderPath(normalized))
    {
        HYP_LOG(Editor, Warning, "Invalid asset folder '{}'", folderPath);
        return;
    }

    bool changed = false;

    {
        Mutex::Guard guard(m_mutex);

        if (normalized.Any())
        {
            changed |= CreateFolder_Internal(normalized);
        }

        for (const AssetPath& assetPath : assetPaths)
        {
            if (!assetPath.IsValid())
            {
                continue;
            }

            if (const auto* entry = m_assetFolders.TryGet(assetPath); entry && entry->second == normalized)
            {
                continue;
            }

            m_assetFolders.Set(assetPath, normalized);

            changed = true;
        }
    }

    if (changed)
    {
        NotifyChanged();
    }
}

void EditorAssetFolders::SetAssetFolderOf(uint32 bucketIndex, Name assetName, const String& folderPath)
{
    if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
    {
        return;
    }

    SetAssetFolder(AssetPath(*AssetBuckets::AllBuckets[bucketIndex], assetName), folderPath);
}

void EditorAssetFolders::RemoveAssetFolder(const AssetPath& assetPath)
{
    bool removed = false;

    {
        Mutex::Guard guard(m_mutex);

        removed = m_assetFolders.Erase(assetPath);
    }

    if (removed)
    {
        NotifyChanged();
    }
}

bool EditorAssetFolders::SyncWithRegistry()
{
    Handle<AssetRegistry> registry = m_registry.Lock();

    if (!registry.IsValid())
    {
        return false;
    }

    Array<AssetPath> registeredAssets;
    Array<AssetDesc> descs;

    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; ++bucketIndex)
    {
        descs.Clear();
        registry->GetBucketAssetDescs(bucketIndex, descs);

        for (const AssetDesc& desc : descs)
        {
            registeredAssets.PushBack(AssetPath(*AssetBuckets::AllBuckets[bucketIndex], desc.name));
        }
    }

    bool changed = false;

    {
        Mutex::Guard guard(m_mutex);

        for (const AssetPath& assetPath : registeredAssets)
        {
            if (m_assetFolders.Contains(assetPath))
            {
                continue;
            }

            const String bucketFolder = GetAssetBucketName(assetPath.bucketIndex);

            CreateFolder_Internal(bucketFolder);
            m_assetFolders.Set(assetPath, bucketFolder);

            changed = true;
        }

        m_hasUnsavedDefaults |= changed;
    }

    if (changed)
    {
        OnFoldersChanged();
    }

    return changed;
}

void EditorAssetFolders::PruneMissingAssets(const AssetRegistry& registry)
{
    Array<AssetPath> assetPaths;

    {
        Mutex::Guard guard(m_mutex);

        for (const auto& it : m_assetFolders)
        {
            assetPaths.PushBack(it.first);
        }
    }

    Array<AssetPath> missingAssets;

    for (const AssetPath& assetPath : assetPaths)
    {
        if (!registry.HasAsset(assetPath.GetBucket(), StringHash(assetPath.assetName)))
        {
            missingAssets.PushBack(assetPath);
        }
    }

    if (missingAssets.Empty())
    {
        return;
    }

    {
        Mutex::Guard guard(m_mutex);

        for (const AssetPath& assetPath : missingAssets)
        {
            m_assetFolders.Erase(assetPath);
        }

        m_hasUnsavedDefaults = true;
    }
}

void EditorAssetFolders::BindRegistry(const Handle<AssetRegistry>& registry)
{
    m_onAssetRenamed.Reset();
    m_registry = registry;

    if (!registry.IsValid())
    {
        return;
    }

    m_onAssetRenamed = registry->OnAssetRenamed.Bind(
        [this](uint32 bucketIndex, Name oldName, Name newName)
        {
            if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
            {
                return;
            }

            const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

            bool moved = false;

            {
                Mutex::Guard guard(m_mutex);

                if (const auto* entry = m_assetFolders.TryGet(AssetPath(bucket, oldName)))
                {
                    const String folder = entry->second;

                    m_assetFolders.Erase(AssetPath(bucket, oldName));
                    m_assetFolders.Set(AssetPath(bucket, newName), folder);

                    moved = true;
                }
            }

            if (moved)
            {
                NotifyChanged();
            }
        });
}

Result EditorAssetFolders::Load(const FilePath& filepath)
{
    {
        Mutex::Guard guard(m_mutex);

        m_folders.Clear();
        m_assetFolders.Clear();
        m_isDirty = false;
        m_hasUnsavedDefaults = false;
    }

    if (!filepath.Exists())
    {
        return {};
    }

    FileByteReader reader { filepath };

    if (reader.Eof() && filepath.FileSize() != 0)
    {
        return HYP_MAKE_ERROR(Error, "Failed to open asset folders file {}", filepath);
    }

    const ByteBuffer buffer = reader.Read();
    const String text = String(buffer.ToByteView());

    enum class Section
    {
        None,
        Folders,
        Assets
    };

    Section section = Section::None;

    Mutex::Guard guard(m_mutex);

    for (const String& rawLine : text.Split('\n'))
    {
        const String line = rawLine.ReplaceAll("\r", "");

        if (line.Trimmed().Empty())
        {
            continue;
        }

        if (line.Trimmed() == "[Folders]")
        {
            section = Section::Folders;
            continue;
        }

        if (line.Trimmed() == "[Assets]")
        {
            section = Section::Assets;
            continue;
        }

        if (section == Section::Folders)
        {
            if (!IsValidFolderPath(line))
            {
                HYP_LOG(Editor, Warning, "Skipping invalid folder '{}' in {}", line, filepath);
                continue;
            }

            CreateFolder_Internal(NormalizeFolderPath(line));
        }
        else if (section == Section::Assets)
        {
            const Array<String> fields = line.Split('\t');

            const AssetPath assetPath = AssetPath(ANSIStringView(*fields[0].Trimmed()));

            if (!assetPath.IsValid())
            {
                HYP_LOG(Editor, Warning, "Skipping unknown asset '{}' in {}", fields[0], filepath);
                continue;
            }

            const String folder = fields.Size() > 1 ? NormalizeFolderPath(fields[1]) : String::empty;

            if (!IsValidFolderPath(folder))
            {
                HYP_LOG(Editor, Warning, "Skipping invalid folder '{}' for asset '{}' in {}", fields[1], fields[0], filepath);
                continue;
            }

            if (folder.Any())
            {
                CreateFolder_Internal(folder);
            }

            m_assetFolders.Set(assetPath, folder);
        }
    }

    return {};
}

Result EditorAssetFolders::Save(const FilePath& filepath)
{
    Array<String> folderLines;
    Array<String> assetLines;

    {
        Mutex::Guard guard(m_mutex);

        if (!m_isDirty && !m_hasUnsavedDefaults && filepath.Exists())
        {
            return {};
        }

        for (const String& folder : m_folders)
        {
            folderLines.PushBack(folder);
        }

        for (const auto& it : m_assetFolders)
        {
            assetLines.PushBack(MakeAssetKey(it.first) + "\t" + it.second);
        }
    }

    std::sort(folderLines.Begin(), folderLines.End());
    std::sort(assetLines.Begin(), assetLines.End());

    String text = "[Folders]\n";

    for (const String& line : folderLines)
    {
        text += line;
        text += "\n";
    }

    text += "[Assets]\n";

    for (const String& line : assetLines)
    {
        text += line;
        text += "\n";
    }

    FileByteWriter writer { filepath };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open asset folders file {} for writing", filepath);
    }

    writer.WriteString(text);
    writer.Close();

    Mutex::Guard guard(m_mutex);

    m_isDirty = false;
    m_hasUnsavedDefaults = false;

    return {};
}

bool EditorAssetFolders::IsDirty() const
{
    Mutex::Guard guard(m_mutex);

    return m_isDirty;
}

void EditorAssetFolders::NotifyChanged()
{
    {
        Mutex::Guard guard(m_mutex);

        m_isDirty = true;
    }

    OnFoldersChanged();
}

} // namespace Hyperion
