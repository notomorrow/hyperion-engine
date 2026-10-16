/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDefaults.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/BlobStorage.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <System/MessageBox.hpp>

#ifdef HYP_EDITOR
#include <Scene/World.hpp>
#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

struct CookingContext {};

class BlobStorageCookCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(BlobStorageCookCommandlet);

public:
    virtual ~BlobStorageCookCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "project",
                "c",
                "Project to content to cook for",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "engine-only",
                "",
                "If true, only engine content will be cooked (minimal needed cache/content to start a game)",
                CommandLineArgumentFlags::NONE,
                {},
                false);

            s_definitions.Add(
                "worlds",
                "",
                "Comma-separated World asset names to cook game content for (all Worlds in the project when empty)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "max-texture-size",
                "",
                "Largest width or height a cooked 2D texture keeps; the mips above it are left out (0 keeps every mip)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "engine-assets",
                "",
                "File listing the Engine assets to cook, one Bucket/Name per line, as written by a run with --record-engine-assets (every Engine asset when empty)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "shader-target",
                "",
                "Platform to compile shaders for and include them in the cook.",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "target-config",
                "",
                "The target's EngineConfig json",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "out-cache",
                "c",
                "Directory to write cache to",
                CommandLineArgumentFlags::REQUIRED,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "out-content",
                "c",
                "Directory to write content to",
                CommandLineArgumentFlags::REQUIRED,
                {},
                JSON::Value(""));

            s_definitions.Add(
                "out-engine-content",
                "",
                "Directory to write engine content to, when it shouldn't share the game's content directory",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    static FilePath GetDirectory(const String& value, bool mkdirs)
    {
        FilePath dir = (value.StartsWith(".")
                    // Relative path - starts with . (eg "../Foo" or "./Foo")
                    ? (CoreApi::GetBaseDirectory() / value)
                    // Just use provided path.
                    : value);

        // eg "Projects/Foo" when not launched from the repo root
        if (!mkdirs && !dir.IsDirectory() && (CoreApi::GetBaseDirectory() / value).IsDirectory())
        {
            dir = CoreApi::GetBaseDirectory() / value;
        }

        if (!dir.IsDirectory() && (!mkdirs || !dir.MkDir()))
        {
            return FilePath();
        }

        return dir;
    }

    virtual Result Run(const CommandLineArguments& args) override
    {
        if (!g_shaderManager)
        {
            g_shaderManager = new ShaderManager;
        }

        GlobalContextScope contextScope { CookingContext() };

        Handle<AssetRegistry> gameRegistry;

        const String projectArg = args["project"].ToString();
        FilePath projectDir;

#if 0 //def HYP_EDITOR
        // If we're initializing from editor, eg Build>Import Game Content, we wont have `project` arg,
        // instead we will pull from active project
        if (g_editorState.IsValid())
        {
            if (Handle<EditorProject> currentProject = g_editorState->GetCurrentProject(); currentProject.IsValid())
            {
                if (currentProject->IsSaved())
                {
                    // If its already been saved then save the project again first so assets are totally up to date
                    Result saveResult = currentProject->Save();
                    if (saveResult.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());
                        return saveResult.GetError();
                    }
                }
                else
                {
                    // Not saved, alert the user that we need them to save the project before this:
                    bool cancel = false;
                    Result saveResult;

                    SystemMessageBox(MessageBoxType::INFO)
                        .Title("Must be saved before cooking game content")
                        .Text("The current project is not yet saved - would you like to save the project to continue with the cook task?")
                        .Button("Save", [currentProject, &saveResult]
                        {
                            saveResult = currentProject->Save();
                            if (saveResult.HasError())
                            {
                                HYP_LOG(Editor, Error, "Failed to save project: {}", saveResult.GetError().GetMessage());

                                SystemMessageBox(MessageBoxType::CRITICAL)
                                            .Title("Project could not be saved")
                                            .Text(String("The project could not be saved: ") + saveResult.GetError().GetMessage()
                                                + "\nThe operation will be aborted to prevent loss of data")
                                            .Button("OK", NoOpFunction<void> {})
                                            .Show();
                                            
                            }
                        })
                        .Button("Discard", NoOpFunction<void> {})
                        .Button("Cancel", [&cancel] { cancel = true; })
                        .Show();

                    if (saveResult.HasError())
                    {
                        return saveResult.GetError();
                    }

                    if (cancel)
                    {
                        return {}; // ok, intentional cancel
                    }
                }

                projectDir = currentProject->GetFilePath().BasePath();
            }
        }
#endif // HYP_EDITOR

        const bool engineOnly = args["engine-only"].ToBool(false);
        if (!engineOnly)
        {
            if (projectArg.Empty())
            {
                return HYP_MAKE_ERROR(Error, "No valid project directory provided (required unless --engine-only is true)");
            }

            if ((projectDir = GetDirectory(projectArg, false)); projectDir.Empty())
            {
                return HYP_MAKE_ERROR(Error, "Package path is non existant or is not a directory: {}", projectArg);
            }

            gameRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, projectDir);
            gameRegistry->Initialize(nullptr);
        }

        Handle<AssetRegistry> engineRegistry;
        engineRegistry = GetEngineAssetRegistry();

        if (!engineRegistry.IsValid())
        {
            engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            engineRegistry->Initialize(nullptr);
        }

        HYP_LOG(Assets, Info, "Cooking blob storage");

        const FilePath outCacheDir = GetDirectory(args["out-cache"].ToString(), true);
        if (outCacheDir.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create cache directory: {}", args["out-cache"].ToString());
        }

        const FilePath outContentDir = GetDirectory(args["out-content"].ToString(), true);
        if (outContentDir.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create content directory: {}", args["out-content"].ToString());
        }

        // engine manifests can go to their own directory, for builds that keep engine content apart from the game's (<base>/Content/Engine)
        FilePath outEngineContentDir = outContentDir;

        if (const String outEngineContentArg = args["out-engine-content"].ToString(); outEngineContentArg.Any())
        {
            outEngineContentDir = GetDirectory(outEngineContentArg, true);

            if (outEngineContentDir.Empty())
            {
                return HYP_MAKE_ERROR(Error, "Failed to create engine content directory: {}", outEngineContentArg);
            }
        }

        Array<String> worldNames;

        for (const String& worldName : args["worlds"].ToString().Split(','))
        {
            if (worldName.Trimmed().Any())
            {
                worldNames.PushBack(worldName.Trimmed());
            }
        }

        const String engineAssetListArg = args["engine-assets"].ToString();

        s_maxTextureSize = 0;
        StringUtil::Parse(args["max-texture-size"].ToString(), &s_maxTextureSize);

        Array<Handle<AssetObject>> targetShaderAssets;

        if (const String shaderTargetArg = args["shader-target"].ToString(); shaderTargetArg.Any())
        {
            SetEngineAssetRegistry(engineRegistry);

            if (Result compileResult = CompileTargetShaders(engineRegistry, shaderTargetArg, FilePath(args["target-config"].ToString()), targetShaderAssets); compileResult.HasError())
            {
                return compileResult;
            }
        }

        Result result = Cook(engineRegistry, gameRegistry, projectDir, worldNames, FilePath(engineAssetListArg), targetShaderAssets, outCacheDir, outContentDir, outEngineContentDir);

        if (result.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to cook blob storage: {}", result.GetError().GetMessage());

            return result;
        }

        HYP_LOG(Assets, Info, "Blob storage cook complete.");

        return {};
    }

private:
      struct CollectedBlob
    {
        uint32 bucketIndex;
        StringHash key;
        const char* magic;
        uint16 version;
        size_t size;
        BlobDataReference* reference;
    };

    static inline uint32 s_maxTextureSize = 0;

    // Leaves out the mips above the size limit. The texture keeps the rest of its chain as it was stored.
    static void LimitTextureSize(Texture& texture, uint32 maxTextureSize)
    {
        const TextureDesc textureDesc = texture.GetTextureDesc();

        if (textureDesc.type != TextureType::Texture2D || textureDesc.numLayers != 1 || !textureDesc.HasStoredMips())
        {
            return;
        }

        const uint32 numMips = textureDesc.NumMips();

        Vec3u extent = textureDesc.extent;
        uint32 numMipsRemoved = 0;

        while (MathUtil::Max(extent.x, extent.y) > maxTextureSize && numMipsRemoved + 1 < numMips && textureDesc.mipOffsets[numMipsRemoved] != 0)
        {
            extent = Vec3u(MathUtil::Max(extent.x >> 1, 1u), MathUtil::Max(extent.y >> 1, 1u), 1u);

            ++numMipsRemoved;
        }

        if (numMipsRemoved == 0)
        {
            return;
        }

        const uint32 keptDataStart = textureDesc.mipOffsets[numMipsRemoved - 1];

        ByteBuffer keptData;

        {
            auto readScope = texture.GetReadScope();

            const ConstByteView imageData = texture.GetImageData();

            if (!imageData || imageData.Size() <= keptDataStart)
            {
                return;
            }

            keptData = ByteBuffer(imageData.Slice(keptDataStart));
        }

        TextureDesc limitedDesc = textureDesc;
        limitedDesc.extent = extent;

        for (uint32 mipIndex = 0; mipIndex < TextureDesc::MaxMips; mipIndex++)
        {
            const uint32 sourceIndex = mipIndex + numMipsRemoved;

            limitedDesc.mipOffsets[mipIndex] = (sourceIndex < TextureDesc::MaxMips && textureDesc.mipOffsets[sourceIndex] != 0)
                ? textureDesc.mipOffsets[sourceIndex] - keptDataStart
                : 0;
        }

        auto writeScope = texture.GetWriteScope();

        texture.SetTextureDesc(limitedDesc);
        texture.SetImageData(keptData.ToByteView());
    }

    static Result CookAsset(
        const FilePath& outputContentDir,
        const Handle<AssetObject>& assetObject,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes,
        Set<const AssetObject*>& cookedAssets)
    {
        if (!assetObject.IsValid())
        {
            return {};
        }

        if (assetObject->IsTransient())
        {
            return {};
        }

        if (!cookedAssets.Insert(assetObject.Get()).second)
        {
            return {};
        }

        if (s_maxTextureSize != 0 && assetObject->GetPath().GetBucket() == AssetBuckets::Textures)
        {
            LimitTextureSize(*static_cast<Texture*>(assetObject.Get()), s_maxTextureSize);
        }

        const uint32 bucketIndex = assetObject->GetPath().GetBucket().GetIndex();
        const Name assetName = assetObject->GetPath().GetName();

        const FilePath bucketContentDir = outputContentDir / GetAssetBucketName(bucketIndex);

        if (!bucketContentDir.Exists() && !bucketContentDir.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create bucket content directory '{}'", bucketContentDir);
        }

        readLocks.PushBack(assetObject->GetReadScope());

        const FilePath manifestPath = bucketContentDir / (String(*assetName) + ".hmf");

        FileByteWriter manifestWriter { manifestPath };

        if (!manifestWriter.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open manifest file '{}' for writing", manifestPath);
        }

        if (Result manifestResult = assetObject->SaveManifest(manifestWriter); manifestResult.HasError())
        {
            return manifestResult;
        }

        manifestWriter.Close();

        Array<Tuple<const char*, uint16, BlobDataReference*>> blobDataReferences;
        assetObject->CollectBlobDataReferences(blobDataReferences);

        for (auto& tup : blobDataReferences)
        {
            BlobDataReference* reference = tup.GetElement<2>();

            if (!reference->raw || reference->size == 0)
            {
                continue;
            }

            const size_t headerOffset = ByteUtil::AlignAs(blockSizes[bucketIndex], alignof(BlobHeader));
            blockSizes[bucketIndex] = headerOffset + sizeof(BlobHeader) + reference->size;

            collectedBlobs.PushBack(CollectedBlob {
                bucketIndex,
                StringHash(reference->key),
                tup.GetElement<0>(),
                tup.GetElement<1>(),
                size_t(reference->size),
                reference
            });
        }

        return {};
    }

    static Result CookBucketInFull(
        AssetRegistry* registry,
        uint32 bucketIndex,
        const FilePath& outputContentDir,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes,
        Set<const AssetObject*>& cookedAssets)
    {
        const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

        Array<AssetDesc> assetDescs;
        registry->GetBucketAssetDescs(bucketIndex, assetDescs);

        for (const AssetDesc& assetDesc : assetDescs)
        {
            Handle<AssetObject> assetObject = registry->GetAsset(bucket, assetDesc.name);

            if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
            {
                return result;
            }
        }

        return {};
    }

    static bool IsConditionMet(const JSON::Value& targetConfig, const String& condition)
    {
        if (condition.Empty() || !targetConfig.IsObject())
        {
            return true;
        }

        const auto value = targetConfig.Get(condition);

        if (value.IsObject())
        {
            const auto enabled = value["Enabled"];

            return enabled.IsNullOrUndefined() || enabled.ToBool();
        }

        return value.IsNullOrUndefined() || value.ToBool();
    }

    static Result CompileTargetShaders(
        const Handle<AssetRegistry>& engineRegistry,
        const String& target,
        const FilePath& targetConfigPath,
        Array<Handle<AssetObject>>& outAssets)
    {
        if (target != "web")
        {
            return HYP_MAKE_ERROR(Error, "Unknown shader target '{}', expected web", target);
        }

        JSON::Value targetConfig;

        if (targetConfigPath.Any())
        {
            FileByteReader configReader { targetConfigPath };

            if (configReader.Eof())
            {
                return HYP_MAKE_ERROR(Error, "Failed to read target config '{}'", targetConfigPath);
            }

            JSON::ParseResult configParseResult = JSON::Parse(String(configReader.Read().ToByteView()));

            if (!configParseResult.ok)
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse target config '{}': {}", targetConfigPath, configParseResult.message);
            }

            targetConfig = std::move(configParseResult.value);
        }

        const FilePath shadersPath = EngineGlobals::GetConfigDirectory() / "Shaders.hmf";

        FileByteReader shadersReader { shadersPath };

        HMF::ErrorList errorList;
        HMF::ParseResult parseResult = HMF::Parse(shadersPath, shadersReader, &errorList);

        if (parseResult.HasError() || !parseResult.GetValue().Is<ShaderDefinitions>())
        {
            return HYP_MAKE_ERROR(Error, "Failed to read shader definitions from '{}'", shadersPath);
        }

        const ShaderDefinitions& definitions = parseResult.GetValue().Get<ShaderDefinitions>();

        Array<String> passShaderNames;

        for (const ShaderDefinition& definition : definitions.definitions)
        {
            if (definition.cookKind != ShaderCookKind::Pass || !IsConditionMet(targetConfig, definition.condition))
            {
                continue;
            }

            passShaderNames.PushBack(String(*definition.name));
        }

        HYP_LOG(Assets, Info, "Compiling {} pass shader(s) of {} for {}", passShaderNames.Size(), definitions.definitions.Size(), target);

        if (!TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().Start();
        }

        if (!g_shaderCompiler)
        {
            g_shaderCompiler = new ShaderCompiler;
        }

        ShaderCompileParams params;
        params.SetTargetPlatforms(ShaderCompileTargetPlatform::Web);
        params.SetTargetBackends(ShaderCompileTargetBackend::WebGPU);
        params.shaderFilters = passShaderNames;
        params.exactShaderFilters = true;

        if (!g_shaderCompiler->Initialize(/* precompileShaders */ true, params))
        {
            HYP_LOG(Assets, Warning, "Some pass shaders failed to compile for {} (see the errors above); the package goes without them", target);
        }

        const ShaderPropertyId backendPropertyId = ShaderPropertyDictionary::GetInstance().Intern(ShaderProperty(NAME("BACKEND"), NAME("WEBGPU")));
        const ShaderPropertyId targetPropertyId = ShaderPropertyDictionary::GetInstance().Intern(ShaderProperty(NAME("TARGET"), NAME("WEB")));

        uint32 numVariants = 0;

        for (const String& shaderName : passShaderNames)
        {
            Handle<AssetObject> bundleAsset = engineRegistry->GetAsset(AssetBuckets::ShaderBundles, StringHash(shaderName));

            if (!bundleAsset.IsValid())
            {
                HYP_LOG(Assets, Warning, "No shader bundle for '{}' after compiling it for {}", shaderName, target);
                continue;
            }

            outAssets.PushBack(bundleAsset);

            for (const Handle<Shader>& shader : static_cast<const ShaderBundle*>(bundleAsset.Get())->compiledShaders)
            {
                if (shader.IsValid() && shader->properties.Test(backendPropertyId) && shader->properties.Test(targetPropertyId))
                {
                    outAssets.PushBack(shader);
                    ++numVariants;
                }
            }
        }

        HYP_LOG(Assets, Info, "{} pass shader variant(s) for {}", numVariants, target);

        return {};
    }

    static Result CookListedAssets(
        AssetRegistry* registry,
        const FilePath& assetListPath,
        const Array<Handle<AssetObject>>& targetShaderAssets,
        const FilePath& outputContentDir,
        Array<TSharedResLock<AssetObject>>& readLocks,
        Array<CollectedBlob>& collectedBlobs,
        Array<uint64>& blockSizes,
        Set<const AssetObject*>& cookedAssets,
        const Set<uint8>& projectMeshLayouts)
    {
        FILE* listFile = nullptr;

        if (assetListPath.Any())
        {
            listFile = fopen(assetListPath.Data(), "r");

            if (listFile == nullptr)
            {
                return HYP_MAKE_ERROR(Error, "Failed to open asset list '{}'", assetListPath);
            }
        }

        HYP_DEFER({
            if (listFile != nullptr)
            {
                fclose(listFile);
            }
        });

        Set<String> seenEntries;
        Array<Handle<AssetObject>> listedAssets;
        Set<const AssetObject*> listedAssetSet;
        uint32 numMissing = 0;

        for (const Handle<AssetObject>& assetObject : targetShaderAssets)
        {
            if (assetObject.IsValid() && listedAssetSet.Insert(assetObject.Get()).second)
            {
                listedAssets.PushBack(assetObject);
            }
        }

        char line[1024];

        while (listFile != nullptr && fgets(line, sizeof(line), listFile) != nullptr)
        {
            const String entry = String(line).Trimmed();

            if (entry.Empty() || !seenEntries.Insert(entry).second)
            {
                continue;
            }

            const Array<String> parts = entry.Split('/');

            if (parts.Size() != 2)
            {
                HYP_LOG(Assets, Warning, "Skipping asset list entry '{}', expected Bucket/Name", entry);

                continue;
            }

            const AssetBucket& bucket = GetAssetBucketByName(StringHash(parts[0]));

            if (bucket == AssetBuckets::None)
            {
                HYP_LOG(Assets, Warning, "Skipping asset list entry '{}', no such bucket", entry);

                continue;
            }

            Handle<AssetObject> assetObject = registry->GetAsset(bucket, StringHash(parts[1]));

            if (!assetObject.IsValid())
            {
                HYP_LOG(Assets, Warning, "Listed asset '{}' is not in the registry", entry);

                ++numMissing;

                continue;
            }

            if (listedAssetSet.Insert(assetObject.Get()).second)
            {
                listedAssets.PushBack(std::move(assetObject));
            }
        }

        // meshes the engine builds at run time (terrain, debug draw, particles, MeshBuilder shapes) are all VT_Simple
        Set<uint8> meshLayouts = projectMeshLayouts;
        meshLayouts.Insert(uint8(VT_Simple));

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            if (assetObject->GetPath().GetBucket() == AssetBuckets::Meshes)
            {
                meshLayouts.Insert(static_cast<const Mesh*>(assetObject.Get())->GetMeshAttributes().inputLayout.mask);
            }
        }

        Set<const AssetObject*> unusedLayoutShaders;

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            if (assetObject->GetPath().GetBucket() != AssetBuckets::ShaderBundles)
            {
                continue;
            }

            ShaderBundle* shaderBundle = static_cast<ShaderBundle*>(assetObject.Get());

            for (auto it = shaderBundle->compiledShaders.Begin(); it != shaderBundle->compiledShaders.End();)
            {
                if (!listedAssetSet.Contains(static_cast<const AssetObject*>(it->Get())))
                {
                    it = shaderBundle->compiledShaders.Erase(it);

                    continue;
                }

                ++it;
            }

            // a request compiles every layout the bundle's optional attributes allow, but only the cooked meshes' layouts are drawn
            const bool drawsMeshLayouts = AnyOf(shaderBundle->compiledShaders, [&meshLayouts](const Handle<Shader>& shader)
                {
                    return meshLayouts.Contains(shader->inputLayout.mask);
                });

            if (!drawsMeshLayouts)
            {
                continue;
            }

            for (auto it = shaderBundle->compiledShaders.Begin(); it != shaderBundle->compiledShaders.End();)
            {
                if (!meshLayouts.Contains((*it)->inputLayout.mask))
                {
                    unusedLayoutShaders.Insert(static_cast<const AssetObject*>(it->Get()));

                    it = shaderBundle->compiledShaders.Erase(it);

                    continue;
                }

                ++it;
            }
        }

        for (const Handle<AssetObject>& assetObject : listedAssets)
        {
            if (unusedLayoutShaders.Contains(assetObject.Get()))
            {
                continue;
            }

            if (Result result = CookAsset(outputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
            {
                return result;
            }
        }

        HYP_LOG(Assets, Info, "Cooked {} listed asset(s) from \"{}\" and the {} shader asset(s) compiled for the target, {} missing, {} shader variant(s) left out for vertex layouts no cooked mesh has",
            listedAssets.Size() - unusedLayoutShaders.Size(), assetListPath, targetShaderAssets.Size(), numMissing, unusedLayoutShaders.Size());

        return {};
    }

    static Result Cook(
        const Handle<AssetRegistry>& engineRegistry, const Handle<AssetRegistry>& gameRegistry,
        const FilePath& projectPath,
        const Array<String>& worldNames,
        const FilePath& engineAssetListPath,
        const Array<Handle<AssetObject>>& targetShaderAssets,
        const FilePath& outputCacheDir, const FilePath& outputContentDir, const FilePath& outputEngineContentDir)
    {
        Array<TSharedResLock<AssetObject>> readLocks;
        Array<CollectedBlob> collectedBlobs;

        Array<uint64> blockSizes;
        blockSizes.Resize(MaxAssetBuckets);

        Set<const AssetObject*> cookedAssets;

        // the project's walk reaches engine assets, so the engine registry has to be ready before it
        if (engineRegistry)
        {
            SetEngineAssetRegistry(engineRegistry);

            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { engineRegistry } };
            engineRegistry->LoadAssetDescs();
        }

        Array<Handle<AssetObject>> assetsToCook;
        Set<uint8> projectMeshLayouts;

#ifdef HYP_EDITOR
        // Game content: only cook what's actually reachable from the project's own data to strip assets that are unused.
        if (gameRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { gameRegistry } };
            gameRegistry->LoadAssetDescs();

            TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectPath);

            if (loadProjectResult.HasValue())
            {
                Set<AssetObject*> seenAssets;

                auto callback = [&assetsToCook, &seenAssets](const Handle<AssetObject>& assetObject)
                    {
                        if (!assetObject.IsValid())
                        {
                            return;
                        }

                        if (!seenAssets.Insert(assetObject.Get()).second)
                        {
                            return;
                        }

                        assetsToCook.PushBack(assetObject);
                    };

                // Loop over all World assets, grab stuff that's reachable from them.
                constexpr AssetBucket WorldsBucket = AssetBuckets::Worlds;

                Array<AssetDesc> assetDescs;
                gameRegistry->GetBucketAssetDescs(WorldsBucket.GetIndex(), assetDescs);

                static const Name s_terrainLayerClassName = NAME("TerrainWorldGridLayer");

                bool usesTerrain = false;

                for (const AssetDesc& assetDesc : assetDescs)
                {
                    if (worldNames.Any() && !worldNames.Contains(String(*assetDesc.name)))
                    {
                        continue;
                    }

                    Handle<AssetObject> worldAsset = gameRegistry->GetAsset(WorldsBucket, assetDesc.name);
                    if (!worldAsset.IsValid())
                    {
                        HYP_LOG(Assets, Warning, "Failed to load World asset \"{}\"", assetDesc.name);
                        continue;
                    }

                    AssetRegistry::WalkAssetDeep(BoxedValue(worldAsset), callback);

                    const Handle<World>& world = DynamicCast<World>(worldAsset);

                    if (world.IsValid() && world->GetWorldGrid().IsValid())
                    {
                        for (const Handle<WorldGridLayer>& layer : world->GetWorldGrid()->GetLayers())
                        {
                            usesTerrain |= layer.IsValid() && layer->InstanceClass()->GetName() == s_terrainLayerClassName;
                        }
                    }
                }

                HYP_LOG(Assets, Info, "Found {} asset(s) reachable from project at \"{}\"", assetsToCook.Size(), projectPath);

                // the engine assets engine code uses whatever the content is; the terrain ones only for a project with terrain
                if (engineRegistry)
                {
                    GlobalContextScope engineContextScope { AssetRegistryContext { engineRegistry } };

                    EngineDefaults engineDefaults = EngineDefaults::Get();

                    if (!usesTerrain)
                    {
                        engineDefaults.terrain = EngineTerrainDefaults {};
                    }

                    AssetRegistry::WalkAssetDeep(BoxedValue(engineDefaults), callback);
                }

                for (const String& entry : loadProjectResult.GetValue()->GetPlatformSettings().cook.alwaysCook)
                {
                    const Array<String> parts = entry.Split('/');
                    const AssetBucket& bucket = parts.Any() ? GetAssetBucketByName(StringHash(parts[0])) : AssetBuckets::None;

                    if (bucket == AssetBuckets::None || parts.Size() > 2)
                    {
                        HYP_LOG(Assets, Warning, "Skipping AlwaysCook entry '{}', expected Bucket or Bucket/Name", entry);
                        continue;
                    }

                    if (parts.Size() == 2)
                    {
                        Handle<AssetObject> asset = gameRegistry->GetAsset(bucket, StringHash(parts[1]));

                        if (!asset.IsValid())
                        {
                            HYP_LOG(Assets, Warning, "AlwaysCook entry '{}' is not in the project", entry);
                            continue;
                        }

                        AssetRegistry::WalkAssetDeep(BoxedValue(asset), callback);

                        continue;
                    }

                    Array<AssetDesc> bucketAssetDescs;
                    gameRegistry->GetBucketAssetDescs(bucket.GetIndex(), bucketAssetDescs);

                    for (const AssetDesc& bucketAssetDesc : bucketAssetDescs)
                    {
                        AssetRegistry::WalkAssetDeep(BoxedValue(gameRegistry->GetAsset(bucket, bucketAssetDesc.name)), callback);
                    }
                }

                HYP_LOG(Assets, Info, "{} asset(s) to cook with the engine defaults{} and AlwaysCook", assetsToCook.Size(), usesTerrain ? " (terrain included)" : "");
            }
            else
            {
                HYP_LOG(Assets, Warning, "Failed to load EditorProject from \"{}\" ({}), falling back to cooking every asset in the registry",
                    projectPath, loadProjectResult.GetError().GetMessage());

                for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
                {
                    Array<AssetDesc> assetDescs;
                    gameRegistry->GetBucketAssetDescs(bucketIndex, assetDescs);

                    const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

                    for (const AssetDesc& assetDesc : assetDescs)
                    {
                        assetsToCook.PushBack(gameRegistry->GetAsset(bucket, assetDesc.name));
                    }
                }
            }

            for (const Handle<AssetObject>& assetObject : assetsToCook)
            {
                if (assetObject.IsValid() && assetObject->GetPath().GetBucket() == AssetBuckets::Meshes)
                {
                    projectMeshLayouts.Insert(static_cast<const Mesh*>(assetObject.Get())->GetMeshAttributes().inputLayout.mask);
                }
            }
        }
#endif // HYP_EDITOR

        if (engineRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { engineRegistry } };

            if (engineAssetListPath.Any() || targetShaderAssets.Any())
            {
                if (Result result = CookListedAssets(engineRegistry, engineAssetListPath, targetShaderAssets, outputEngineContentDir, readLocks, collectedBlobs, blockSizes, cookedAssets, projectMeshLayouts); result.HasError())
                {
                    return result;
                }
            }
            else
            {
                for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
                {
                    if (Result result = CookBucketInFull(engineRegistry, bucketIndex, outputEngineContentDir, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
                    {
                        return result;
                    }
                }
            }
        }

#ifdef HYP_EDITOR
        if (gameRegistry)
        {
            GlobalContextScope assetRegistryContextScope { AssetRegistryContext { gameRegistry } };

            uint32 numEngineAssets = 0;

            for (const Handle<AssetObject>& assetObject : assetsToCook)
            {
                if (!assetObject.IsValid())
                {
                    continue;
                }

                const FilePath* assetOutputContentDir = &outputContentDir;

                switch (assetObject->GetPath().registryId)
                {
                case AssetRegistryId::Game:
                    break;
                case AssetRegistryId::Engine:
                    assetOutputContentDir = &outputEngineContentDir;
                    ++numEngineAssets;
                    break;
                default:
                    HYP_LOG(Assets, Warning, "Skipping \"{}\", reached from the project but owned by the {} registry",
                        assetObject->GetPath(), GetAssetRegistryName(assetObject->GetPath().registryId));
                    continue;
                }

                if (Result result = CookAsset(*assetOutputContentDir, assetObject, readLocks, collectedBlobs, blockSizes, cookedAssets); result.HasError())
                {
                    return result;
                }
            }

            HYP_LOG(Assets, Info, "{} of the reachable asset(s) are engine content, cooked to \"{}\"", numEngineAssets, outputEngineContentDir);
        }
#endif // HYP_EDITOR

        Array<BlobBlockInfo> blocks;

        for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; bucketIndex++)
        {
            if (blockSizes[bucketIndex] == 0)
            {
                continue;
            }

            blocks.PushBack(BlobBlockInfo { bucketIndex, blockSizes[bucketIndex] });
        }

        BlobStorage cookedStorage;
        cookedStorage.Lock(outputCacheDir, /* readOnly */ false);

        bool locked = true;
        HYP_DEFER({
            if (locked)
            {
                cookedStorage.Unlock();
            }
        });

        if (Result result = cookedStorage.BeginCook(blocks); result.HasError())
        {
            return result;
        }

        for (const CollectedBlob& collectedBlob : collectedBlobs)
        {
            if (collectedBlob.reference->raw == nullptr || collectedBlob.reference->size != collectedBlob.size)
            {
                return HYP_MAKE_ERROR(Error, "Blob data for key {} became invalid between collection and write (asset was unpaged or reallocated during cooking)", collectedBlob.key.GetHashCode().Value());
            }

            BlobHeader header {};

            const size_t magicLength = collectedBlob.magic ? std::strlen(collectedBlob.magic) : 0;
            Memory::Copy((char*)header.magic, collectedBlob.magic, MathUtil::Min(magicLength, sizeof(header.magic)));
            header.payloadOffset = 0;
            header.payloadSize = collectedBlob.size;
            header.version = collectedBlob.version;

            if (!cookedStorage.PutData(collectedBlob.bucketIndex, collectedBlob.key, header, collectedBlob.reference->raw))
            {
                return HYP_MAKE_ERROR(Error, "Failed to write cooked blob data for key {}", collectedBlob.key.GetHashCode().Value());
            }
        }

        if (Result result = cookedStorage.FinishCook(); result.HasError())
        {
            return result;
        }

        cookedStorage.Unlock();
        locked = false;

        // Write shader cache here as well
        HYP_LOG(Assets, Info, "Writing shader cache data...");
        g_shaderManager->WriteShaderCache(outputCacheDir);
        HYP_LOG(Assets, Info, "Shader cache data written.");

        // For webGPU, collecting minimal pipelines
        const FilePath pipelineManifestPath = EngineGlobals::GetCacheDirectory() / "pipelinevariants.bin";

        if (pipelineManifestPath.Exists())
        {
            FileByteReader pipelineManifestReader { pipelineManifestPath };
            const ByteBuffer pipelineManifest = pipelineManifestReader.Read();
            pipelineManifestReader.Close();

            FileByteWriter pipelineManifestWriter { outputCacheDir / "pipelinevariants.bin" };
            pipelineManifestWriter.Write(pipelineManifest.Data(), pipelineManifest.Size());
            pipelineManifestWriter.Close();
        }

        return {};
    }
};

HYP_EXPORT const Class* g_clsBlobStorageCookCommandlet = nullptr;

const Class* BlobStorageCookCommandlet::StaticClass()
{
    return g_clsBlobStorageCookCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(BlobStorageCookCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "blobstoragecook"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(BlobStorageCookCommandlet);

} // namespace Hyperion
