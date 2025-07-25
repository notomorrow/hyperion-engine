/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetRegistry.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <system/MessageBox.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

HYP_API WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage)
{
    if (!assetPackage.IsValid())
    {
        return {};
    }

    return assetPackage->GetName();
}

HYP_API WeakName AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return {};
    }

    return assetObject->GetName();
}

#pragma region AssetResourceBase

void AssetDataResourceBase::Initialize()
{
    Mutex::Guard guard(m_mutex);
    if (m_writingThread != ThreadId::Invalid())
    {
        HYP_LOG(Assets, Error, "Asset is being written to by thread {}, cannot read", m_writingThread.GetName());

        return;
    }

    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    // @TODO Loading async
    // @TODO: non-filepath loading (load from packfile)
    const FilePath assetFilePath = assetObject->GetFilePath();

    if (assetFilePath.Empty() || !assetFilePath.Exists())
    {
        HYP_LOG(Assets, Error, "Asset file path is invalid or does not exist: {}", assetFilePath);

        return;
    }

    HypData value;

    FBOMReader reader { FBOMReaderConfig {} };
    FBOMResult err;

    if ((err = reader.LoadFromFile(assetFilePath, value)))
    {
        HYP_LOG(Assets, Error, "Failed to load asset at path: {}\n\tMessage: {}", assetFilePath, err.message);

        return;
    }

    Extract_Internal(std::move(value));
}

void AssetDataResourceBase::Destroy()
{
    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    Unload_Internal();
}

Result AssetDataResourceBase::Save()
{
    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    return SaveTo(assetObject->GetFilePath());
}

Result AssetDataResourceBase::SaveTo(const FilePath& path)
{
    Mutex::Guard guard(m_mutex);

    if (m_writingThread != ThreadId::Invalid())
    {
        return HYP_MAKE_ERROR(Error, "Asset is already being written to by thread {}", m_writingThread.GetName());
    }

    m_writingThread = ThreadId::Current();
    HYP_DEFER({ m_writingThread = ThreadId::Invalid(); });

    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    FileByteWriter byteWriter { path };
    HYP_DEFER({ byteWriter.Close(); });

    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetAssetTypeId());
    Assert(marshal != nullptr);

    FBOMObject object;

    AnyRef assetRef = GetAssetRef();

    if (!assetRef)
    {
        return HYP_MAKE_ERROR(Error, "Asset data reference is invalid!");
    }

    if (FBOMResult err = marshal->Serialize(assetRef, object))
    {
        return HYP_MAKE_ERROR(Error, "Failed to serialize asset: {}", err.message);
    }

    Assert(object.GetType().GetNativeTypeId() == GetAssetTypeId(),
        "Object must have a native TypeId associated to be deserialized properly! Expected TypeId {}, Got serialized type: {}",
        GetAssetTypeId().Value(),
        object.GetType().ToString(true));

    writer.Append(std::move(object));

    if (FBOMResult err = writer.Emit(&byteWriter))
    {
        return HYP_MAKE_ERROR(Error, "Failed to write asset to disk");
    }

    HYP_LOG(Assets, Debug, "Saved asset to '{}'", path);

    return {};
}

#pragma endregion AssetResourceBase

#pragma region AssetObject

AssetObject::AssetObject()
    : m_resource(&GetNullResource())
{
}

AssetObject::AssetObject(Name name)
    : m_name(name),
      m_resource(&GetNullResource())
{
}

AssetObject::~AssetObject()
{
    if (m_resource != nullptr && !m_resource->IsNull())
    {
        FreeResource(m_resource);
    }
}

Result AssetObject::Rename(Name name)
{
    if (name == m_name)
    {
        // same name, do nothing
        return {};
    }

    if (Handle<AssetPackage> package = GetPackage())
    {
        Handle<AssetObject> strongThis = HandleFromThis();

        if (Result result = package->RemoveAssetObject(strongThis); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to remove asset object '{}' from package '{}': {}", m_name, package->GetName(), result.GetError().GetMessage());

            return result;
        }

        m_name = name;

        if (Result result = package->AddAssetObject(strongThis); result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to rename asset object '{}' to '{}': {}", m_name, name, result.GetError().GetMessage());

            return result;
        }
    }
    else
    {
        m_name = name;
    }

    return {};
}

FilePath AssetObject::GetFilePath() const
{
    AssertDebug(IsRegistered(), "Calling GetPath() on an unregistered asset object");

    if (!m_assetPath.IsValid())
    {
        return {};
    }

    return g_assetManager->GetBasePath() / m_assetPath.ToString();
}

bool AssetObject::IsLoaded() const
{
    if (!m_resource || m_resource->IsNull())
    {
        return false;
    }

    return static_cast<AssetDataResourceBase*>(m_resource)->IsInitialized();
}

Result AssetObject::Save() const
{
    if (!m_resource || m_resource->IsNull())
    {
        return HYP_MAKE_ERROR(Error, "No resource set, cannot save");
    }

    return static_cast<AssetDataResourceBase*>(m_resource)->Save();
}

void AssetObject::Init()
{
    if (m_resource && !m_resource->IsNull())
    {
        static_cast<AssetDataResourceBase*>(m_resource)->m_assetObject = WeakHandleFromThis();
    }

    SetReady(true);
}

#pragma endregion AssetObject

#pragma region AssetPackage

AssetPackage::AssetPackage()
    : AssetPackage(Name::Invalid())
{
}

AssetPackage::AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags)
    : m_name(name),
      m_flags(flags)
{
    if (name.IsValid())
    {
        const char* str = name.LookupString();
        if (str[0] == '$')
        {
            m_flags |= APF_TRANSIENT | APF_HIDDEN;
        }

        String friendlyNameStr;

        for (auto it : UTF8StringView(str))
        {
            if (utf::utf32Isalpha(it) || utf::utf32Isdigit(it))
            {
                friendlyNameStr.Append(it);
            }
        }

        m_friendlyName = CreateNameFromDynamicString(StringUtil::ToPascalCase(friendlyNameStr, true));
    }
}

void AssetPackage::SetAssetObjects(const AssetObjectSet& assetObjects)
{
    if (IsInitCalled())
    {
        AssetObjectSet previousAssetObjects;

        { // store so we can call OnAssetObjectRemoved outside of the lock
            Mutex::Guard guard(m_mutex);

            previousAssetObjects = std::move(m_assetObjects);
        }

        for (const Handle<AssetObject>& assetObject : previousAssetObjects)
        {
            assetObject->m_package.Reset();

            OnAssetObjectRemoved(assetObject, true);

            Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

            while (parentPackage.IsValid())
            {
                parentPackage->OnAssetObjectRemoved(assetObject, false);
                parentPackage = parentPackage->GetParentPackage().Lock();
            }
        }

        previousAssetObjects.Clear();
    }

    Array<Handle<AssetObject>> newAssetObjects;

    {
        Mutex::Guard guard(m_mutex);

        m_assetObjects = assetObjects;

        newAssetObjects.Reserve(m_assetObjects.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            assetObject->m_package = WeakHandleFromThis();
            assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

            newAssetObjects.PushBack(assetObject);
        }
    }

    if (IsInitCalled())
    {
        for (const Handle<AssetObject>& assetObject : newAssetObjects)
        {
            InitObject(assetObject);

            if (!IsTransient())
            {
                // save the file in our package
                Result saveAssetResult = assetObject->Save();

                if (saveAssetResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
                }
            }

            OnAssetObjectAdded(assetObject, true);

            Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

            while (parentPackage.IsValid())
            {
                parentPackage->OnAssetObjectAdded(assetObject, false);
                parentPackage = parentPackage->GetParentPackage().Lock();
            }
        }
    }
}

Result AssetPackage::AddAssetObject(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    if (assetObject->GetPackage() == this)
    {
        // already added, fine
        return {};
    }

    if (assetObject->GetPackage().IsValid())
    {
        HYP_LOG(Assets, Warning, "AssetObject '{}' already belongs to another package!", assetObject->GetName());
    }

    assetObject->m_package = WeakHandleFromThis();
    assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

    {
        Mutex::Guard guard(m_mutex);

        auto assetObjectsIt = m_assetObjects.Find(assetObject->GetName());

        if (assetObjectsIt != m_assetObjects.End())
        {
            if (*assetObjectsIt != assetObject)
            {
                return HYP_MAKE_ERROR(Error, "AssetObject with name '{}' already exists in package '{}'", assetObject->GetName(), m_name);
            }

            // already exists, fine
            return {};
        }

        m_assetObjects.Insert({ assetObject });
    }

    if (IsInitCalled())
    {
        InitObject(assetObject);

        if (!IsTransient())
        {
            // save the file in our package
            Result saveAssetResult = assetObject->Save();

            if (saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());

                return HYP_MAKE_ERROR(Error, "Failed to save asset object '{}': {}", assetObject->GetName(), saveAssetResult.GetError().GetMessage());
            }
        }

        OnAssetObjectAdded(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }

    return {};
}

Result AssetPackage::RemoveAssetObject(const Handle<AssetObject>& assetObject)
{
    if (!assetObject)
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    {
        Mutex::Guard guard(m_mutex);

        auto it = m_assetObjects.Find(assetObject->GetName());

        if (it == m_assetObjects.End())
        {
            return HYP_MAKE_ERROR(Error, "AssetObject '{}' not found in package '{}'", assetObject->GetName(), m_name);
        }

        m_assetObjects.Erase(it);

        assetObject->m_package.Reset();
        assetObject->m_assetPath = {};
    }

    if (IsInitCalled())
    {
        OnAssetObjectRemoved(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectRemoved(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }

        /// TODO: remove the file
    }

    return {};
}

FilePath AssetPackage::GetAbsolutePath() const
{
    return g_assetManager->GetBasePath() / BuildPackagePath();
}

String AssetPackage::BuildPackagePath() const
{
    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    if (!parentPackage.IsValid())
    {
        return *GetFriendlyName();
    }

    return parentPackage->BuildPackagePath() + "/" + *GetFriendlyName();
}

AssetPath AssetPackage::BuildAssetPath(Name assetName) const
{
    if (!assetName.IsValid())
    {
        return {};
    }

    Array<Name> chain;

    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    while (parentPackage.IsValid())
    {
        chain.PushBack(parentPackage->GetFriendlyName());

        parentPackage = parentPackage->GetParentPackage().Lock();
    }

    chain.Reverse();
    chain.PushBack(m_name);
    chain.PushBack(assetName);

    AssetPath assetPath;
    assetPath.SetChain(chain);

    return assetPath;
}

void AssetPackage::Init()
{
    Array<Handle<AssetObject>> assetObjects;
    Array<Handle<AssetPackage>> subpackages;

    {
        Mutex::Guard guard(m_mutex);

        assetObjects.Reserve(m_assetObjects.Size());
        subpackages.Reserve(m_subpackages.Size());

        for (const Handle<AssetObject>& assetObject : m_assetObjects)
        {
            InitObject(assetObject);

            assetObjects.PushBack(assetObject);
        }

        for (const Handle<AssetPackage>& subpackage : m_subpackages)
        {
            InitObject(subpackage);

            subpackages.PushBack(subpackage);
        }
    }

    for (const Handle<AssetObject>& assetObject : assetObjects)
    {
        if (!IsTransient())
        {
            // save the file in our package
            Result saveAssetResult = assetObject->Save();

            if (saveAssetResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save asset object '{}' in package '{}': {}", assetObject->GetName(), m_name, saveAssetResult.GetError().GetMessage());
            }
        }

        OnAssetObjectAdded(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }

    SetReady(true);
}

Result AssetPackage::Save()
{
    HYP_SCOPE;
    AssertReady();

    if (IsTransient())
    {
        return HYP_MAKE_ERROR(Error, "Cannot save transient AssetPackage '{}'", m_name);
    }

    Handle<AssetRegistry> registry = m_registry.Lock();

    if (!registry)
    {
        return HYP_MAKE_ERROR(Error, "AssetPackage '{}' does not have a valid AssetRegistry", m_name);
    }

    HYP_LOG(Assets, Debug, "Saving asset package {} with {} assets (path: {})", GetName(), GetAssetObjects().Size(), GetAbsolutePath());

    Mutex::Guard guard(m_mutex);

    FilePath filePath = GetAbsolutePath();

    if (!filePath.Exists())
    {
        if (!filePath.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create package directory '{}'", filePath);
        }
    }
    else if (!filePath.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' already exists and is not a directory", filePath);
    }

    for (const Handle<AssetPackage>& subpackage : m_subpackages)
    {
        if (subpackage->IsTransient())
        {
            continue;
        }

        Result result = subpackage->Save();

        if (result.HasError())
        {
            return result.GetError();
        }
    }

    for (const Handle<AssetObject>& assetObject : m_assetObjects)
    {
        HYP_LOG(Assets, Debug, "Saving asset object '{}' in package '{}'", assetObject->GetName(), m_name);

        if (!assetObject->IsLoaded())
        {
            HYP_LOG(Assets, Warning, "AssetObject '{}' is not loaded, skipping save", assetObject->GetName());
            continue;
        }

        Result result = assetObject->Save();

        if (result.HasError())
        {
            return result.GetError();
        }
    }

    return {};
}

#pragma endregion AssetPackage

#pragma region AssetRegistry

AssetRegistry::AssetRegistry()
    : AssetRegistry("res")
{
}

AssetRegistry::AssetRegistry(const String& rootPath)
    : m_rootPath(rootPath)
{
}

void AssetRegistry::Init()
{
    HYP_SCOPE;

#ifdef HYP_EDITOR
    // Add transient package for imported assets in editor mode
    Handle<AssetPackage> importsPackage = GetPackageFromPath("$Import", true);

    const FilePath importedPath = importsPackage->GetAbsolutePath();

    if (!importedPath.Exists())
    {
        if (importedPath.MkDir())
        {
            HYP_LOG(Assets, Debug, "Created directory for imported assets: {}", importedPath);
        }
        else
        {
            HYP_LOG(Assets, Error, "Failed to create directory for imported assets: {}", importedPath);
        }
    }
    else if (!importedPath.IsDirectory())
    {
        HYP_LOG(Assets, Error, "Path '{}' already exists and is not a directory", importedPath);
    }

    importsPackage->OnAssetObjectAdded
        .Bind([this, importedPath](const Handle<AssetObject>& assetObject, bool isDirect)
            {
                if (!assetObject->GetResource() || assetObject->GetResource()->IsNull())
                {
                    HYP_LOG(Assets, Error, "AssetObject '{}' has no resource, skipping import", assetObject->GetName());

                    return;
                }

                // build relative dir
                Array<Name> parts;

                Handle<AssetPackage> currentPackage = assetObject->GetPackage();
                while (currentPackage.IsValid() && !currentPackage->IsTransient())
                {
                    parts.PushBack(currentPackage->GetFriendlyName());

                    currentPackage = currentPackage->GetParentPackage().Lock();
                }

                parts.Reverse();

                FilePath dir = importedPath / String::Join(parts, '/', &Name::LookupString);

                if (!dir.Exists())
                {
                    if (!dir.MkDir())
                    {
                        HYP_LOG(Assets, Error, "Failed to create directory for imported assets: {}", dir);

                        return;
                    }
                }
                else if (!dir.IsDirectory())
                {
                    HYP_LOG(Assets, Error, "Path '{}' already exists and is not a directory", dir);

                    return;
                }

                FilePath path = dir / *assetObject->GetName();

                AssetDataResourceBase* resourceCasted = static_cast<AssetDataResourceBase*>(assetObject->GetResource());
                ResourceHandle resourceHandle(*resourceCasted, false);

                Result saveResult = resourceCasted->Save();

                if (saveResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save imported asset '{}': {}", assetObject->GetName(), saveResult.GetError().GetMessage());
                }
                else
                {
                    HYP_LOG(Assets, Debug, "Saved imported asset '{}' to '{}'", assetObject->GetName(), path);
                }
            })
        .Detach();
#endif

    AssetPackageSet packages;

    {
        Mutex::Guard guard(m_mutex);
        packages = m_packages;
    }

    for (const Handle<AssetPackage>& package : packages)
    {
        InitObject(package);
    }

    SetReady(true);
}

AssetRegistry::~AssetRegistry()
{
}

FilePath AssetRegistry::GetAbsolutePath() const
{
    HYP_SCOPE;

    // Allow temporary override of the root path
    AssetRegistryRootPathContext* context = GetGlobalContext<AssetRegistryRootPathContext>();

    if (context)
    {
        return context->value;
    }

    Mutex::Guard guard(m_mutex);
    return FilePath::Current() / m_rootPath;
}

void AssetRegistry::SetRootPath(const String& rootPath)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    m_rootPath = rootPath;
}

void AssetRegistry::SetPackages(const AssetPackageSet& packages)
{
    HYP_SCOPE;

    Proc<void(Handle<AssetPackage>)> initializePackage;

    // Set up the parent package pointer for a package, so all subpackages can trace back to their parent
    // and call OnPackageAdded for each nested package
    initializePackage = [this, &initializePackage](const Handle<AssetPackage>& package)
    {
        Assert(package.IsValid());

        package->m_registry = WeakHandleFromThis();

        if (IsInitCalled())
        {
            InitObject(package);
            OnPackageAdded(package);
        }

        for (const Handle<AssetPackage>& subpackage : package->m_subpackages)
        {
            subpackage->m_parentPackage = package;

            initializePackage(subpackage);
        }
    };

    {
        Mutex::Guard guard(m_mutex);

        for (const Handle<AssetPackage>& package : packages)
        {
            Assert(package.IsValid());

            m_packages.Set(package);
        }
    }

    for (const Handle<AssetPackage>& package : packages)
    {
        initializePackage(package);
    }
}

Result AssetRegistry::RegisterAsset(const UTF8StringView& path, const Handle<AssetObject>& assetObject)
{
    HYP_SCOPE;

    if (!assetObject.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AssetObject is invalid");
    }

    String pathString = path;
    Array<String> pathStringSplit = pathString.Split('/', '\\');

    String assetName;

    pathString = String::Join(pathStringSplit, '/');

    AssetRegistryPathType pathType = AssetRegistryPathType::ASSET;

    if (assetObject->GetName().IsValid())
    {
        pathType = AssetRegistryPathType::PACKAGE;
    }

    Handle<AssetPackage> assetPackage;

    {
        assetPackage = GetPackageFromPath_Internal(pathString, pathType, /* createIfNotExist */ true, assetName);

        if (pathType == AssetRegistryPathType::ASSET)
        {
            const Name baseName = assetName.Any() ? CreateNameFromDynamicString(assetName) : NAME("Unnamed");

            assetObject->m_name = assetPackage->GetUniqueAssetName(baseName);
        }
    }

    return assetPackage->AddAssetObject(assetObject);
}

Name AssetRegistry::GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const
{
    HYP_SCOPE;

    Handle<AssetPackage> package = const_cast<AssetRegistry*>(this)->GetPackageFromPath(packagePath, /* createIfNotExist */ false);

    if (!package.IsValid())
    {
        return baseName;
    }

    /// FIXME: Don't consider filesyetem stuff, just check AssetObjects

    return package->GetUniqueAssetName(baseName);
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist)
{
    HYP_SCOPE;

    String assetName;

    return GetPackageFromPath_Internal(path, AssetRegistryPathType::PACKAGE, createIfNotExist, assetName);
}

Handle<AssetPackage> AssetRegistry::GetSubpackage(const Handle<AssetPackage>& parentPackage, Name subpackageName, bool createIfNotExist)
{
    HYP_SCOPE;

    Handle<AssetPackage> subpackage;
    bool isNew = false;

    if (!parentPackage)
    {
        {
            Mutex::Guard guard(m_mutex);

            auto packageIt = m_packages.Find(subpackageName);

            if (createIfNotExist && packageIt == m_packages.End())
            {
                subpackage = CreateObject<AssetPackage>(subpackageName);
                subpackage->m_registry = WeakHandleFromThis();
                subpackage->m_parentPackage = parentPackage;

                m_packages.Insert(subpackage);

                isNew = true;
            }
            else if (packageIt != m_packages.End())
            {
                subpackage = *packageIt;
            }
        }

        if (isNew && subpackage && IsInitCalled())
        {
            InitObject(subpackage);

            OnPackageAdded(subpackage);
        }

        return subpackage;
    }

    {
        Mutex::Guard guard(parentPackage->m_mutex);

        auto packageIt = parentPackage->m_subpackages.Find(subpackageName);

        if (createIfNotExist && packageIt == parentPackage->m_subpackages.End())
        {
            subpackage = CreateObject<AssetPackage>(subpackageName);
            subpackage->m_registry = WeakHandleFromThis();
            subpackage->m_parentPackage = parentPackage;

            parentPackage->m_subpackages.Insert(subpackage);
            isNew = true;
        }
        else if (packageIt != m_packages.End())
        {
            subpackage = *packageIt;
        }
    }

    if (isNew && subpackage && IsInitCalled())
    {
        InitObject(subpackage);

        OnPackageAdded(subpackage);
    }

    return subpackage;
}

bool AssetRegistry::RemovePackage(AssetPackage* package)
{
    HYP_SCOPE;

    if (!package)
    {
        return false;
    }

    if (package->m_registry.GetUnsafe() != this)
    {
        return false;
    }

    package->m_registry.Reset();

    Handle<AssetPackage> strongPackage = package->HandleFromThis();

    bool removed = false;

    {
        Mutex::Guard guard(m_mutex);

        if (package->m_parentPackage.IsValid())
        {
            Handle<AssetPackage> parentPackage = package->m_parentPackage.Lock();

            if (parentPackage.IsValid())
            {
                auto it = parentPackage->m_subpackages.Find(package->GetName());
                Assert(it != parentPackage->m_subpackages.End());

                parentPackage->m_subpackages.Erase(it);

                removed = true;
            }
        }
        else
        {
            auto it = m_packages.Find(package->GetName());
            Assert(it != m_packages.End());

            m_packages.Erase(it);

            removed = true;
        }
    }

    if (removed)
    {
        OnPackageRemoved(strongPackage);

        return true;
    }

    return false;
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath_Internal(const UTF8StringView& path, AssetRegistryPathType pathType, bool createIfNotExist, String& outAssetName)
{
    HYP_SCOPE;

    Handle<AssetPackage> currentPackage;
    String currentString;

    for (auto it = path.Begin(); it != path.End(); ++it)
    {
        if (*it == utf::u32char('/') || *it == utf::u32char('\\'))
        {
            currentPackage = GetSubpackage(currentPackage, CreateNameFromDynamicString(currentString), createIfNotExist);

            currentString.Clear();

            if (!currentPackage)
            {
                return Handle<AssetPackage>::empty;
            }

            continue;
        }

        currentString.Append(*it);
    }

    switch (pathType)
    {
    case AssetRegistryPathType::PACKAGE:
        outAssetName.Clear();

        // If it is a PACKAGE path, if there is any remaining string, get / create the subpackage
        if (!currentPackage.IsValid() || currentString.Any())
        {
            currentPackage = GetSubpackage(currentPackage, CreateNameFromDynamicString(currentString), createIfNotExist);
        }

        return currentPackage;
    case AssetRegistryPathType::ASSET:
        outAssetName = std::move(currentString);

        return currentPackage;
    default:
        HYP_UNREACHABLE();
    }
}

#pragma endregion AssetRegistry

} // namespace hyperion