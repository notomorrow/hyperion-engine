/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/UUID.hpp>
#include <core/utilities/StringView.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/Defines.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/utilities/ForEach.hpp>

#include <Constants.hpp>

#include <algorithm>
#include <type_traits>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetRegistry;
class AssetPackage;
class AssetObject;

HYP_API extern WeakName AssetPackage_KeyByFunction(const Handle<AssetPackage>& assetPackage);
HYP_API extern WeakName AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject);

using AssetPackageSet = HashSet<Handle<AssetPackage>, &AssetPackage_KeyByFunction>;
using AssetObjectSet = HashSet<Handle<AssetObject>, &AssetObject_KeyByFunction>;

class AssetDataResourceBase : public ResourceBase
{
public:
    friend class AssetObject;

    AssetDataResourceBase(const AssetDataResourceBase&) = delete;
    AssetDataResourceBase& operator=(const AssetDataResourceBase&) = delete;

    AssetDataResourceBase(AssetDataResourceBase&&) noexcept = delete;
    AssetDataResourceBase& operator=(AssetDataResourceBase&&) noexcept = delete;

    virtual ~AssetDataResourceBase() override = default;

    ThreadId GetWritingThread() const
    {
        Mutex::Guard guard(m_mutex);
        return m_writingThread;
    }

    Result Save();
    Result SaveTo(const FilePath& path);

protected:
    AssetDataResourceBase() = default;

    virtual void Initialize() override final;
    virtual void Destroy() override final;

    virtual void Update() override
    {
    }

    virtual void Unload_Internal() = 0;

    virtual void Extract_Internal(HypData&& data) = 0;

    virtual TypeId GetAssetTypeId() const = 0;
    virtual AnyRef GetAssetRef() = 0;

    WeakHandle<AssetObject> m_assetObject;
    ThreadId m_writingThread;
    mutable Mutex m_mutex;
};

template <class T>
class AssetDataResource final : public AssetDataResourceBase
{
public:
    AssetDataResource() = default;

    AssetDataResource(const T& data)
        : m_data(data)
    {
    }

    AssetDataResource(T&& data)
        : m_data(std::move(data))
    {
    }

    AssetDataResource(const AssetDataResource&) = delete;
    AssetDataResource& operator=(const AssetDataResource&) = delete;

    AssetDataResource(AssetDataResource&&) noexcept = delete;
    AssetDataResource& operator=(AssetDataResource&&) noexcept = delete;

    virtual ~AssetDataResource() override = default;

protected:
    virtual void Unload_Internal() override
    {
        m_data = {};
    }

    virtual void Extract_Internal(HypData&& data) override
    {
        m_data = std::move(data.Get<T>());
    }

    virtual TypeId GetAssetTypeId() const override
    {
        return const_cast<AssetDataResource*>(this)->GetAssetRef().GetTypeId();
    }

    virtual AnyRef GetAssetRef() override
    {
        return AnyRef(&m_data);
    }

    T m_data;
};

HYP_CLASS(Abstract)
class HYP_API AssetObject : public HypObject<AssetObject>
{
    HYP_OBJECT_BODY(AssetObject);

public:
    friend class AssetRegistry;
    friend class AssetPackage;

    AssetObject();
    AssetObject(Name name);

    template <class T>
    AssetObject(Name name, T&& data)
        : AssetObject(name)
    {
        m_resource = AllocateResource<AssetDataResource<NormalizedType<T>>>(std::forward<T>(data));
        static_cast<AssetDataResource<NormalizedType<T>>*>(m_resource)->m_assetObject = WeakHandleFromThis();
    }

    AssetObject(const AssetObject& other) = delete;
    AssetObject& operator=(const AssetObject& other) = delete;
    AssetObject(AssetObject&& other) noexcept = delete;
    AssetObject& operator=(AssetObject&& other) noexcept = delete;
    ~AssetObject();

    HYP_METHOD()
    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    void SetName(Name name);

    HYP_METHOD()
    HYP_FORCE_INLINE Handle<AssetPackage> GetPackage() const
    {
        return m_package.Lock();
    }

    HYP_FORCE_INLINE IResource* GetResource() const
    {
        return m_resource;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const AssetPath& GetPath() const
    {
        return m_assetPath;
    }

    HYP_METHOD()
    FilePath GetFilePath() const;

    HYP_METHOD()
    bool IsLoaded() const;

    HYP_METHOD()
    Result Save() const;

protected:
    void Init() override;

    template <class T>
    T* GetResourceData() const
    {
        if (!m_resource || m_resource->IsNull())
        {
            return nullptr;
        }

        AssetDataResourceBase* resourceCasted = static_cast<AssetDataResourceBase*>(m_resource);

        if (!resourceCasted->IsInitialized())
        {
            return nullptr;
        }

        AssertDebug(resourceCasted->GetAssetTypeId() == TypeId::ForType<T>(), "Type mismatch!");

        ResourceHandle resourceHandle(*resourceCasted);

        return resourceCasted->GetAssetRef().TryGet<T>();
    }

    HYP_FIELD(Property = "UUID", Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name m_name;

    HYP_FIELD()
    WeakHandle<AssetPackage> m_package;

    HYP_FIELD()
    IResource* m_resource;

    AssetPath m_assetPath;
};

HYP_ENUM()
enum AssetPackageFlags : uint32
{
    APF_NONE = 0x0,
    APF_TRANSIENT = 0x1,
    APF_HIDDEN = 0x2
};

HYP_MAKE_ENUM_FLAGS(AssetPackageFlags);

HYP_CLASS()
class HYP_API AssetPackage final : public HypObject<AssetPackage>
{
    HYP_OBJECT_BODY(AssetPackage);

    friend class AssetRegistry;

public:
    AssetPackage();
    AssetPackage(Name name, EnumFlags<AssetPackageFlags> flags = APF_NONE);
    AssetPackage(const AssetPackage& other) = delete;
    AssetPackage& operator=(const AssetPackage& other) = delete;
    AssetPackage(AssetPackage&& other) noexcept = delete;
    AssetPackage& operator=(AssetPackage&& other) noexcept = delete;
    ~AssetPackage() = default;

    HYP_METHOD()
    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetUUID(const UUID& uuid)
    {
        m_uuid = uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetFriendlyName() const
    {
        return m_friendlyName.IsValid() ? m_friendlyName : m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetFriendlyName(Name friendlyName)
    {
        m_friendlyName = friendlyName;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE EnumFlags<AssetPackageFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsTransient() const
    {
        return m_flags[APF_TRANSIENT];
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsHidden() const
    {
        return m_flags[APF_HIDDEN];
    }

    HYP_FORCE_INLINE const WeakHandle<AssetRegistry>& GetRegistry() const
    {
        return m_registry;
    }

    HYP_FORCE_INLINE const WeakHandle<AssetPackage>& GetParentPackage() const
    {
        return m_parentPackage;
    }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE const AssetPackageSet& GetSubpackages() const
    {
        return m_subpackages;
    }

    /*! \internal Serialization only */
    HYP_FORCE_INLINE void SetSubpackages(const AssetPackageSet& subpackages)
    {
        m_subpackages = subpackages;
    }

    template <class Callback>
    void ForEachSubpackage(Callback&& callback) const
    {
        AssetPackageSet set;

        {
            Mutex::Guard guard(m_mutex);
            set = m_subpackages;
        }

        ForEach(set, std::forward<Callback>(callback));
    }

    HYP_FORCE_INLINE const AssetObjectSet& GetAssetObjects() const
    {
        return m_assetObjects;
    }

    void SetAssetObjects(const AssetObjectSet& assetObjects);

    template <class Callback>
    void ForEachAssetObject(Callback&& callback) const
    {
        AssetObjectSet set;

        {
            Mutex::Guard guard(m_mutex);
            set = m_assetObjects;
        }

        ForEach(set, std::forward<Callback>(callback));
    }

    template <class T>
    Handle<AssetObject> NewAssetObject(Name name, T&& data)
    {
        AddAssetObject(CreateObject<AssetObject>(name, std::forward<T>(data)));
    }

    void AddAssetObject(const Handle<AssetObject>& assetObject);
    void RemoveAssetObject(const Handle<AssetObject>& assetObject);

    HYP_METHOD()
    FilePath GetAbsolutePath() const;

    HYP_METHOD()
    String BuildPackagePath() const;

    HYP_METHOD()
    AssetPath BuildAssetPath(Name assetName) const;

    HYP_METHOD(Scriptable)
    Name GetUniqueAssetName(Name baseName) const;

    HYP_METHOD()
    Result Save();

    Delegate<void, Handle<AssetObject>, bool /* isDirect */> OnAssetObjectAdded;
    Delegate<void, Handle<AssetObject>, bool /* isDirect */> OnAssetObjectRemoved;

private:
    void Init() override;

    Name GetUniqueAssetName_Impl(Name baseName) const
    {
        return baseName;
    }

    HYP_FIELD(Property = "UUID", Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name m_name;

    HYP_FIELD(Property = "FriendlyName", Serialize = true)
    Name m_friendlyName;

    HYP_FIELD(Property="Flags", Serialize = true)
    EnumFlags<AssetPackageFlags> m_flags;

    WeakHandle<AssetRegistry> m_registry;
    WeakHandle<AssetPackage> m_parentPackage;
    AssetPackageSet m_subpackages;
    AssetObjectSet m_assetObjects;
    mutable Mutex m_mutex;
};

enum class AssetRegistryPathType : uint8
{
    PACKAGE = 0,
    ASSET = 1
};

HYP_CLASS()
class HYP_API AssetRegistry final : public HypObject<AssetRegistry>
{
    HYP_OBJECT_BODY(AssetRegistry);

public:
    AssetRegistry();
    AssetRegistry(const String& rootPath);
    AssetRegistry(const AssetRegistry& other) = delete;
    AssetRegistry& operator=(const AssetRegistry& other) = delete;
    AssetRegistry(AssetRegistry&& other) noexcept = delete;
    AssetRegistry& operator=(AssetRegistry&& other) noexcept = delete;
    ~AssetRegistry();

    HYP_METHOD()
    FilePath GetAbsolutePath() const;

    HYP_METHOD()
    HYP_FORCE_INLINE String GetRootPath() const
    {
        Mutex::Guard guard(m_mutex);
        return m_rootPath;
    }

    HYP_METHOD()
    void SetRootPath(const String& rootPath);

    HYP_FORCE_INLINE const AssetPackageSet& GetPackages() const
    {
        return m_packages;
    }

    /*! \internal Serialization only */
    void SetPackages(const AssetPackageSet& packages);

    template <class Callback>
    void ForEachPackage(Callback&& callback) const
    {
        ForEach(m_packages, m_mutex, std::forward<Callback>(callback));
    }

    void RegisterAsset(const UTF8StringView& path, const Handle<AssetObject>& assetObject);
    
    template <class T>
    Handle<AssetObject> NewAssetObject(const UTF8StringView& path, T&& data)
    {
        String pathString = path;
        Array<String> pathStringSplit = pathString.Split('/', '\\');

        String assetName;

        pathString = String::Join(pathStringSplit, '/');

        Mutex::Guard guard1(m_mutex);
        Handle<AssetPackage> assetPackage = GetPackageFromPath_Internal(pathString, AssetRegistryPathType::PACKAGE, /* createIfNotExist */ true, assetName);

        Handle<AssetObject> assetObject = CreateObject<AssetObject>(CreateNameFromDynamicString(assetName), std::forward<T>(data));

        assetPackage->AddAssetObject(assetObject);

        return assetObject;
    }

    HYP_METHOD()
    Name GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const;

    HYP_METHOD()
    Handle<AssetPackage> GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist = true);

    HYP_METHOD()
    Handle<AssetPackage> GetSubpackage(const Handle<AssetPackage>& parentPackage, Name subpackageName, bool createIfNotExist = true);

    HYP_METHOD()
    bool RemovePackage(AssetPackage* package);

    Delegate<void, Handle<AssetPackage>> OnPackageAdded;
    Delegate<void, Handle<AssetPackage>> OnPackageRemoved;

private:
    void Init() override;

    Handle<AssetPackage> GetPackageFromPath_Internal(const UTF8StringView& path, AssetRegistryPathType pathType, bool createIfNotExist, String& outAssetName);

    HYP_FIELD(Property = "RootPath", Serialize = true)
    String m_rootPath;

    AssetPackageSet m_packages;
    mutable Mutex m_mutex;
};

struct AssetRegistryRootPathContext
{
    FilePath value;
};

} // namespace hyperion

