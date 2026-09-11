/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 *
 *  Usage: CookTexturesCommandlet [--input-dir=Data/Textures]
 *    Imports source textures (png/jpg/tga/...) into the engine asset registry and
 *    saves them out as cooked assets (.hmf manifests + blob storage) so the
 *    original files never have to ship with the engine.
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>
#include <Core/FileSystem/FilePath.hpp>
#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Rendering/Texture.hpp>

namespace Hyperion {

static const char* s_cookedTextureExtensions[] = {
    "png", "jpg", "jpeg", "tga", "bmp", "psd", "gif", "hdr", "tif"
};

static bool IsLinearTextureName(const String& lowercaseStem)
{
    const char* keywords[] = {
        "normal", "rough", "metal", "ambient", "_ao", "height", "mask", "splat"
    };

    for (const char* keyword : keywords)
    {
        if (lowercaseStem.Contains(keyword))
        {
            return true;
        }
    }

    return false;
}

class CookTexturesCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(CookTexturesCommandlet);

public:
    virtual ~CookTexturesCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        static bool s_initialized = false;

        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "input-dir",
                "i",
                "Directory containing source textures to cook",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value("Data/Textures"));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        FilePath inputDir = FilePath(args["input-dir"].ToString());

        if (inputDir.Empty())
        {
            inputDir = "Data/Textures";
        }

        if (!inputDir.IsAbsolute())
        {
            inputDir = CoreApi::GetBaseDirectory() / inputDir;
        }

        HYP_LOG(Assets, Info, "CookTexturesCommandlet running (input dir: {})", inputDir);

        if (!inputDir.Exists() || !inputDir.IsDirectory())
        {
            return HYP_MAKE_ERROR(Error, "Input directory does not exist: {}", inputDir);
        }

        Handle<AssetRegistry> registry = GetEngineAssetRegistry();

        if (!registry.IsValid())
        {
            registry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            registry->Initialize(nullptr);

            // set globally so Engine-id assets can resolve their owning registry for blob persistence
            SetEngineAssetRegistry(registry);
        }

        GlobalContextScope contextScope { AssetRegistryContext { registry } };

        uint32 numCooked = 0;
        uint32 numFailed = 0;

        for (const FilePath& filepath : inputDir.GetAllFilesInDirectory())
        {
            const String extensionLower = filepath.GetExtension().ToLower();

            bool isSupported = false;

            for (const char* cookedExtension : s_cookedTextureExtensions)
            {
                if (extensionLower == cookedExtension)
                {
                    isSupported = true;
                    break;
                }
            }

            if (!isSupported)
            {
                continue;
            }

            const bool isLinear = IsLinearTextureName(String(filepath.Basename()).ToLower());

            // color maps are imported as sRGB; data maps (normals, masks, etc) stay linear
            const AssetLoadHint hint = isLinear
                ? AssetLoadHint::NoHint
                : AssetLoadHint::TextureLoader_LoadAsSRGB;

            auto loadResult = g_assetManager->Load<Texture>(filepath, String::empty, hint);

            if (loadResult.HasError())
            {
                HYP_LOG(Assets, Warning, "Failed to cook '{}': {}", filepath, loadResult.GetError().GetMessage());
                numFailed++;
            }
            else
            {
                Handle<Texture> texture = loadResult.GetValue().ExtractAs<Handle<Texture>>();

                HYP_LOG(Assets, Info, "Cooked '{}' ({}x{}, srgb: {})", filepath,
                    texture->GetTextureDesc().extent.x, texture->GetTextureDesc().extent.y,
                    texture->GetTextureDesc().IsSrgb());
                numCooked++;
            }
        }

        HYP_LOG(Assets, Info, "Cook loop done: {} cooked, {} failed", numCooked, numFailed);

        if (numCooked == 0)
        {
            return HYP_MAKE_ERROR(Error, "No textures were cooked - check the input directory");
        }

        registry->SaveDirtyAssets();

        HYP_LOG(Assets, Info, "Cooked {} textures into the engine asset registry ({} failed)", numCooked, numFailed);

        return {};
    }
};

const Class* g_clsCookTexturesCommandlet = nullptr;

const Class* CookTexturesCommandlet::StaticClass()
{
    return g_clsCookTexturesCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(CookTexturesCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "cooktextures"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(CookTexturesCommandlet);

} // namespace Hyperion
