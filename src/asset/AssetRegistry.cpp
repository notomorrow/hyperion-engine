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
    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    // @TODO Loading async
    const FilePath assetFilePath = assetObject->GetFilePath();
    auto result = g_assetManager->Load(GetAssetTypeId(), assetFilePath);

    if (result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset '{}': {}", assetFilePath, result.GetError().GetMessage());
        return;
    }

    Extract_Internal(std::move(result).GetValue());
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

    HYP_LOG(Assets, Debug, "Saving asset {} to path: {}", assetObject->GetName(), assetObject->GetFilePath());

    FileByteWriter byteWriter(assetObject->GetFilePath());
    HYP_DEFER({ byteWriter.Close(); });

    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetAssetTypeId());
    Assert(marshal != nullptr);

    FBOMObject object;

    if (FBOMResult err = marshal->Serialize(GetAssetRef(), object))
    {
        return HYP_MAKE_ERROR(Error, "Failed to serialize asset: {}", err.message);
    }

    writer.Append(std::move(object));

    if (FBOMResult err = writer.Emit(&byteWriter))
    {
        return HYP_MAKE_ERROR(Error, "Failed to write asset to disk");
    }

    HYP_LOG(Assets, Debug, "Saved asset '{}'", assetObject->GetPath());

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

void AssetObject::SetName(Name name)
{
    if (name == m_name)
    {
        return;
    }

    if (Handle<AssetPackage> package = GetPackage())
    {
        Handle<AssetObject> strongThis = HandleFromThis();

        package->RemoveAssetObject(strongThis);

        m_name = name;

        package->AddAssetObject(strongThis);
    }
    else
    {
        m_name = name;
    }
}

FilePath AssetObject::GetFilePath() const
{
    Handle<AssetPackage> package = m_package.Lock();

    if (!package.IsValid())
    {
        return {};
    }

    Handle<AssetRegistry> registry = package->GetRegistry().Lock();

    if (!registry.IsValid())
    {
        return {};
    }

    return registry->GetAbsolutePath() / m_assetPath.ToString();
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
    if (!m_resource || m_resource->IsNull() || !IsLoaded())
    {
        return HYP_MAKE_ERROR(Error, "No resource loaded");
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
    if (name.IsValid() && name.LookupString()[0] == '$')
    {
        m_flags |= APF_TRANSIENT;
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

void AssetPackage::AddAssetObject(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return;
    }

    // already in the package
    if (assetObject->GetPackage() == this)
    {
        return;
    }

    if (assetObject->GetPackage().IsValid())
    {
        HYP_LOG(Assets, Warning, "AssetObject '{}' already belongs to another package!", assetObject->GetName());
    }

    {
        Mutex::Guard guard(m_mutex);

        if (m_assetObjects.Find(assetObject->GetName()) != m_assetObjects.End())
        {
            HYP_LOG(Assets, Warning, "AssetObject '{}' already exists in package '{}'", assetObject->GetName(), m_name);

            return;
        }

        assetObject->m_package = WeakHandleFromThis();
        assetObject->m_assetPath = BuildAssetPath(assetObject->m_name);

        m_assetObjects.Insert({ assetObject });
    }

    if (IsInitCalled())
    {
        InitObject(assetObject);

        OnAssetObjectAdded(assetObject, true);

        Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

        while (parentPackage.IsValid())
        {
            parentPackage->OnAssetObjectAdded(assetObject, false);
            parentPackage = parentPackage->GetParentPackage().Lock();
        }
    }
}

void AssetPackage::RemoveAssetObject(const Handle<AssetObject>& assetObject)
{
    if (!assetObject)
    {
        return;
    }

    {
        Mutex::Guard guard(m_mutex);

        auto it = m_assetObjects.Find(assetObject->GetName());

        if (it == m_assetObjects.End())
        {
            HYP_LOG(Assets, Warning, "AssetObject '{}' not found in package '{}'", assetObject->GetName(), m_name);
            return;
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
    }
}

FilePath AssetPackage::GetAbsolutePath() const
{
    Handle<AssetRegistry> registry = m_registry.Lock();

    if (!registry.IsValid())
    {
        return {};
    }

    return registry->GetAbsolutePath() / BuildPackagePath();
}

String AssetPackage::BuildPackagePath() const
{
    Handle<AssetPackage> parentPackage = m_parentPackage.Lock();

    if (!parentPackage.IsValid())
    {
        return *m_name;
    }

    return parentPackage->BuildPackagePath() + "/" + *m_name;
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
        chain.PushBack(parentPackage->GetName());

        parentPackage = parentPackage->GetParentPackage().Lock();
    }

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
    GetPackageFromPath("$Import", true);
#endif

    Mutex::Guard guard(m_mutex);

    for (const Handle<AssetPackage>& package : m_packages)
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

    Mutex::Guard guard(m_mutex);

    Proc<void(Handle<AssetPackage>)> initializePackage;

    // Set up the parent package pointer for a package, so all subpackages can trace back to their parent
    // and call OnPackageAdded for each nested package
    initializePackage = [this, &initializePackage](const Handle<AssetPackage>& parentPackage)
    {
        Assert(parentPackage.IsValid());

        parentPackage->m_registry = WeakHandleFromThis();

        for (const Handle<AssetPackage>& package : parentPackage->m_subpackages)
        {
            package->m_parentPackage = parentPackage;

            initializePackage(package);
        }

        OnPackageAdded(parentPackage);
    };

    for (const Handle<AssetPackage>& package : packages)
    {
        Assert(package.IsValid());

        m_packages.Set(package);

        initializePackage(package);
    }
}

void AssetRegistry::RegisterAsset(const UTF8StringView& path, const Handle<AssetObject>& assetObject)
{
    HYP_SCOPE;

    if (!assetObject.IsValid())
    {
        return;
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

    Mutex::Guard guard1(m_mutex);
    Handle<AssetPackage> assetPackage = GetPackageFromPath_Internal(pathString, pathType, /* createIfNotExist */ true, assetName);

    if (pathType == AssetRegistryPathType::ASSET)
    {
        const Name baseName = assetName.Any() ? CreateNameFromDynamicString(assetName) : NAME("Unnamed");

        assetObject->m_name = assetPackage->GetUniqueAssetName(baseName);
    }

    assetPackage->AddAssetObject(assetObject);
}

// AnyHandle AssetRegistry::GetAsset(const UTF8StringView &path)
// {
//     Mutex::Guard guard1(m_mutex);

//     String assetName;

//     Handle<AssetPackage> assetPackage = GetPackageFromPath_Internal(path, AssetRegistryPathType::ASSET, /* createIfNotExist */ false, assetName);

//     if (!assetPackage.IsValid()) {
//         return AnyHandle::empty;
//     }

//     Mutex::Guard guard2(assetPackage->m_mutex);
//     auto it = assetPackage->m_assetObjects.Find(WeakName(assetName.Data()));

//     if (it == assetPackage->m_assetObjects.End()) {
//         return AnyHandle::empty;
//     }

//     return (*it)->GetObject();
// }

Name AssetRegistry::GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const
{
    HYP_SCOPE;

    Handle<AssetPackage> package = const_cast<AssetRegistry*>(this)->GetPackageFromPath(packagePath, /* createIfNotExist */ false);

    if (!package.IsValid())
    {
        return baseName;
    }

    return package->GetUniqueAssetName(baseName);
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    String assetName;

    return GetPackageFromPath_Internal(path, AssetRegistryPathType::PACKAGE, createIfNotExist, assetName);
}

const Handle<AssetPackage>& AssetRegistry::GetSubpackage(const Handle<AssetPackage>& parentPackage, Name subpackageName, bool createIfNotExist)
{
    HYP_SCOPE;

    AssetPackageSet* packagesSet = nullptr;

    if (!parentPackage)
    {
        packagesSet = &m_packages;
    }
    else
    {
        packagesSet = &parentPackage->m_subpackages;
    }

    auto packageIt = packagesSet->Find(subpackageName);

    if (createIfNotExist && packageIt == packagesSet->End())
    {
        Handle<AssetPackage> package = CreateObject<AssetPackage>(subpackageName);
        package->m_registry = WeakHandleFromThis();
        package->m_parentPackage = parentPackage;
        InitObject(package);

        packageIt = packagesSet->Insert(package).first;

        OnPackageAdded(package);
    }

    return packageIt != packagesSet->End() ? *packageIt : Handle<AssetPackage>::empty;
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