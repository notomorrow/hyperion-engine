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

class HYP_API AssetResourceBase : public ResourceBase
{
public:
    friend class AssetObject;

    AssetResourceBase() = default;

    template <class T>
    AssetResourceBase(T&& value)
        : m_loadedAsset(std::forward<T>(value))
    {
    }

    AssetResourceBase(const AssetResourceBase&) = delete;
    AssetResourceBase& operator=(const AssetResourceBase&) = delete;

    AssetResourceBase(AssetResourceBase&&) noexcept = delete;
    AssetResourceBase& operator=(AssetResourceBase&&) noexcept = delete;

    virtual ~AssetResourceBase() override = default;

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    virtual void Update() override
    {
    }

    Result Save();

    virtual Result Save_Internal() = 0;

    virtual TypeId GetAssetTypeId() const = 0;

    WeakHandle<AssetObject> m_assetObject;
    LoadedAsset m_loadedAsset;
};

class HYP_API ObjectAssetResourceBase : public AssetResourceBase
{
public:
    ObjectAssetResourceBase() = default;

    ObjectAssetResourceBase(HypData&& value)
    {
        m_assetTypeId = value.GetTypeId();
        m_loadedAsset = std::move(value);
    }

    template <class T, typename = std::enable_if_t<!isHypData<NormalizedType<T>>>>
    ObjectAssetResourceBase(T&& value)
        : AssetResourceBase(std::forward<T>(value)),
          m_assetTypeId(TypeId::ForType<T>())
    {
    }

    ObjectAssetResourceBase(const ObjectAssetResourceBase&) = delete;
    ObjectAssetResourceBase& operator=(const ObjectAssetResourceBase&) = delete;

    ObjectAssetResourceBase(ObjectAssetResourceBase&&) noexcept = delete;
    ObjectAssetResourceBase& operator=(ObjectAssetResourceBase&&) noexcept = delete;

    virtual ~ObjectAssetResourceBase() override = default;

protected:
    virtual Result Save_Internal() override;

    virtual TypeId GetAssetTypeId() const override final
    {
        return m_assetTypeId;
    }

    TypeId m_assetTypeId;
};

HYP_CLASS()
class HYP_API AssetObject final : public HypObject<AssetObject>
{
    HYP_OBJECT_BODY(AssetObject);

public:
    friend class AssetRegistry;
    friend class AssetPackage;

    AssetObject();
    AssetObject(Name name);

    AssetObject(Name name, HypData&& data)
        : AssetObject(name)
    {
        m_resource = AllocateResource<ObjectAssetResourceBase>(std::move(data));
    }

    template <class T, typename = std::enable_if_t<!isHypData<NormalizedType<T>>>>
    AssetObject(Name name, T&& value)
        : AssetObject(name)
    {
        m_resource = AllocateResource<ObjectAssetResourceBase>(std::forward<T>(value));
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
    HYP_FORCE_INLINE Handle<AssetPackage> GetPackage() const
    {
        return m_package.Lock();
    }

    HYP_FORCE_INLINE AssetResourceBase* GetResource() const
    {
        return m_resource;
    }

    HYP_METHOD()
    String GetPath() const;

    HYP_METHOD()
    FilePath GetFilePath() const;

    HYP_METHOD()
    bool IsLoaded() const;

    HYP_METHOD()
    Result Save() const;

private:
    void Init() override;

    HYP_FIELD(Property = "UUID", Serialize = true)
    UUID m_uuid;

    HYP_FIELD(Property = "Name", Serialize = true)
    Name m_name;

    HYP_FIELD()
    WeakHandle<AssetPackage> m_package;

    HYP_FIELD()
    AssetResourceBase* m_resource;
};

HYP_CLASS()
class HYP_API AssetPackage final : public HypObject<AssetPackage>
{
    HYP_OBJECT_BODY(AssetPackage);

    friend class AssetRegistry;

public:
    AssetPackage();
    AssetPackage(Name name);
    AssetPackage(const AssetPackage& other) = delete;
    AssetPackage& operator=(const AssetPackage& other) = delete;
    AssetPackage(AssetPackage&& other) noexcept = delete;
    AssetPackage& operator=(AssetPackage&& other) noexcept = delete;
    ~AssetPackage() = default;

    HYP_METHOD(Property = "UUID", Serialize = true)
    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD(Property = "UUID", Serialize = true)
    HYP_FORCE_INLINE void SetUUID(const UUID& uuid)
    {
        m_uuid = uuid;
    }

    HYP_METHOD(Property = "Name", Serialize = true)
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name", Serialize = true)
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
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

    HYP_FORCE_INLINE const AssetObjectSet& GetAssetObjects() const
    {
        return m_assetObjects;
    }

    void SetAssetObjects(const AssetObjectSet& assetObjects);

    template <class Callback>
    void ForEachAssetObject(Callback&& callback) const
    {
        ForEach(m_assetObjects, m_mutex, std::forward<Callback>(callback));
    }

    Handle<AssetObject> NewAssetObject(Name name, HypData&& data);

    HYP_METHOD()
    FilePath GetAbsolutePath() const;

    HYP_METHOD()
    String BuildPackagePath() const;

    HYP_METHOD()
    String BuildAssetPath(Name assetName) const;

    HYP_METHOD(Scriptable)
    Name GetUniqueAssetName(Name baseName) const;

    Delegate<void, AssetObject*> OnAssetObjectAdded;
    Delegate<void, AssetObject*> OnAssetObjectRemoved;

private:
    void Init() override;

    Name GetUniqueAssetName_Impl(Name baseName) const
    {
        return baseName;
    }

    UUID m_uuid;
    Name m_name;
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
    ~AssetRegistry() = default;

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

    void RegisterAsset(const UTF8StringView& path, HypData&& data);

    HYP_METHOD()
    Name GetUniqueAssetName(const UTF8StringView& packagePath, Name baseName) const;

    HYP_METHOD()
    Handle<AssetPackage> GetPackageFromPath(const UTF8StringView& path, bool createIfNotExist = true);

    Delegate<void, const Handle<AssetPackage>&> OnPackageAdded;

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

