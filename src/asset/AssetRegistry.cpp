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

void AssetResourceBase::Initialize()
{
    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    // @TODO Loading async w/ AssetBatch
    const FilePath assetFilePath = assetObject->GetFilePath();
    auto result = g_assetManager->Load(GetAssetTypeId(), assetFilePath);

    if (result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset '{}': {}", assetFilePath, result.GetError().GetMessage());
        return;
    }

    m_loadedAsset = std::move(result.GetValue());
}

void AssetResourceBase::Destroy()
{
    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    m_loadedAsset = {};
}

Result AssetResourceBase::Save()
{
    Result result;

    Execute([this, &result]()
        {
            result = Save_Internal();
        });

    return result;
}

#pragma endregion AssetResourceBase

#pragma region ObjectAssetResourceBase

Result ObjectAssetResourceBase::Save_Internal()
{
    Handle<AssetObject> assetObject = m_assetObject.Lock();
    Assert(assetObject.IsValid());

    FileByteWriter byteWriter(assetObject->GetFilePath());

    HYP_DEFER({ byteWriter.Close(); });

    FBOMWriter writer { FBOMWriterConfig {} };

    FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(GetAssetTypeId());
    Assert(marshal != nullptr);

    FBOMObject object;

    if (FBOMResult err = marshal->Serialize(m_loadedAsset.value.ToRef(), object))
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

#pragma endregion ObjectAssetResourceBase

#pragma region AssetObject

AssetObject::AssetObject()
    : m_resource(nullptr)
{
}

AssetObject::AssetObject(Name name)
    : m_name(name),
      m_resource(nullptr)
{
}

AssetObject::~AssetObject()
{
    if (m_resource != nullptr)
    {
        FreeResource(m_resource);
    }
}

String AssetObject::GetPath() const
{
    Handle<AssetPackage> package = m_package.Lock();

    if (!package.IsValid())
    {
        return *m_name;
    }

    return package->BuildAssetPath(m_name);
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

    return registry->GetAbsolutePath() / package->BuildAssetPath(m_name);
}

bool AssetObject::IsLoaded() const
{
    if (!m_resource)
    {
        return false;
    }

    bool isLoaded = false;

    m_resource->Execute([resource = m_resource, &isLoaded]()
        {
            if (resource->m_loadedAsset.IsValid())
            {
                isLoaded = true;
            }
        });

    return isLoaded;
}

Result AssetObject::Save() const
{
    if (!m_resource || !IsLoaded())
    {
        return HYP_MAKE_ERROR(Error, "No resource loaded");
    }

    return m_resource->Save();
}

void AssetObject::Init()
{
    if (m_resource != nullptr)
    {
        m_resource->m_assetObject = WeakHandleFromThis();
    }

    SetReady(true);
}

#pragma endregion AssetObject

#pragma region AssetPackage

AssetPackage::AssetPackage()
{
}

AssetPackage::AssetPackage(Name name)
    : m_name(name)
{
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

            OnAssetObjectRemoved(assetObject.Get());
        }

        previousAssetObjects.Clear();
    }

    Array<Handle<AssetObject>> newAssetObjects;

    {
        Mutex::Guard guard(m_mutex);

        m_assetObjects = assetObjects;

        if (IsInitCalled())
        {
            newAssetObjects.Reserve(m_assetObjects.Size());

            for (const Handle<AssetObject>& assetObject : m_assetObjects)
            {
                assetObject->m_package = WeakHandleFromThis();

                InitObject(assetObject);

                newAssetObjects.PushBack(assetObject);
            }
        }
    }

    for (const Handle<AssetObject>& assetObject : newAssetObjects)
    {
        OnAssetObjectAdded(assetObject.Get());
    }
}

Handle<AssetObject> AssetPackage::NewAssetObject(Name name, HypData&& data)
{
    name = GetUniqueAssetName(name);

    Handle<AssetObject> assetObject;

    { // so we can call OnAssetObjectAdded outside of the lock
        Mutex::Guard guard(m_mutex);

        assetObject = CreateObject<AssetObject>(name, std::move(data));
        assetObject->m_package = WeakHandleFromThis();

        m_assetObjects.Insert({ assetObject });

        if (IsInitCalled())
        {
            InitObject(assetObject);
        }
    }

    if (IsInitCalled())
    {
        OnAssetObjectAdded(assetObject.Get());
    }

    return assetObject;
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

String AssetPackage::BuildAssetPath(Name assetName) const
{
    return BuildPackagePath() + "/" + *assetName;
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
        OnAssetObjectAdded(assetObject.Get());
    }

    SetReady(true);
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
    const auto addPackage = [this](Name name, WeakName parentPackageName = Name::Invalid())
    {
        Handle<AssetPackage> package = CreateObject<AssetPackage>(name);
        package->m_registry = WeakHandleFromThis();

        if (parentPackageName.IsValid())
        {
            auto parentPackageIt = m_packages.Find(parentPackageName);
            Assert(parentPackageIt != m_packages.End(), "No package with name '%s' found", Name(parentPackageName).LookupString());

            package->m_parentPackage = *parentPackageIt;

            (*parentPackageIt)->m_subpackages.Insert(std::move(package));

            return;
        }

        m_packages.Insert(std::move(package));
    };

    addPackage(NAME("Media"));
    addPackage(NAME("Textures"), "Media");
    addPackage(NAME("Models"), "Media");
    addPackage(NAME("Materials"), "Media");

    addPackage(NAME("Scripts"));
    addPackage(NAME("Config"));
}

void AssetRegistry::Init()
{
    Mutex::Guard guard(m_mutex);

    for (const Handle<AssetPackage>& package : m_packages)
    {
        InitObject(package);
    }

    SetReady(true);
}

FilePath AssetRegistry::GetAbsolutePath() const
{
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
        m_packages.Set(package);

        initializePackage(package);
    }
}

void AssetRegistry::RegisterAsset(const UTF8StringView& path, HypData&& data)
{
    HYP_SCOPE;

    String pathString = path;
    Array<String> pathStringSplit = pathString.Split('/', '\\');

    String assetName;

    pathString = String::Join(pathStringSplit, '/');

    Mutex::Guard guard1(m_mutex);
    Handle<AssetPackage> assetPackage = GetPackageFromPath_Internal(pathString, AssetRegistryPathType::PACKAGE, /* createIfNotExist */ true, assetName);

    assetPackage->NewAssetObject(CreateNameFromDynamicString(assetName), std::move(data));
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
    Handle<AssetPackage> package = const_cast<AssetRegistry*>(this)->GetPackageFromPath(packagePath, /* createIfNotExist */ false);

    if (!package.IsValid())
    {
        return baseName;
    }

    return package->GetUniqueAssetName(baseName);
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist)
{
    Mutex::Guard guard(m_mutex);

    String assetName;

    return GetPackageFromPath_Internal(path, AssetRegistryPathType::PACKAGE, createIfNotExist, assetName);
}

Handle<AssetPackage> AssetRegistry::GetPackageFromPath_Internal(const UTF8StringView& path, AssetRegistryPathType pathType, bool createIfNotExist, String& outAssetName)
{
    HYP_SCOPE;

    Handle<AssetPackage> currentPackage;
    String currentString;

    const auto getSubpackage = [this, createIfNotExist](const Handle<AssetPackage>& parentPackage, UTF8StringView str) -> const Handle<AssetPackage>&
    {
        AssetPackageSet* packagesSet = nullptr;

        if (!parentPackage)
        {
            packagesSet = &m_packages;
        }
        else
        {
            packagesSet = &parentPackage->m_subpackages;
        }

        auto packageIt = packagesSet->Find(str);

        if (createIfNotExist && packageIt == packagesSet->End())
        {
            Handle<AssetPackage> package = CreateObject<AssetPackage>(CreateNameFromDynamicString(str.Data()));
            package->m_registry = WeakHandleFromThis();
            package->m_parentPackage = parentPackage;
            InitObject(package);

            packageIt = packagesSet->Insert(std::move(package)).first;

            OnPackageAdded(package);
        }

        return packageIt != packagesSet->End() ? *packageIt : Handle<AssetPackage>::empty;
    };

    for (auto it = path.Begin(); it != path.End(); ++it)
    {
        if (*it == utf::u32char('/') || *it == utf::u32char('\\'))
        {
            currentPackage = getSubpackage(currentPackage, currentString);

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
            currentPackage = getSubpackage(currentPackage, currentString);
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