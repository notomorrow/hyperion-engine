#include "mtl_material_loader.h"
#include "../../engine.h"

namespace hyperion::v2 {

using Tokens = std::vector<std::string>;
using MtlMaterialLoader = LoaderObject<MaterialLibrary, LoaderFormat::MTL_MATERIAL_LIBRARY>::Loader;

template <class Vector>
static Vector ReadVector(const Tokens &tokens, size_t offset = 1)
{
    Vector result{0.0f};

    int value_index = 0;

    for (size_t i = offset; i < tokens.size(); i++) {
        const auto &token = tokens[i];

        if (token.empty()) {
            continue;
        }

        result.values[value_index++] = float(std::atof(token.c_str()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMaterial(MtlMaterialLoader::Object &object, const std::string &tag)
{
    std::string unique_tag(tag);
    int counter = 0;

    while (std::any_of(
        object.materials.begin(),
        object.materials.end(),
        [&unique_tag](const auto &mesh) { return mesh.tag == unique_tag; }
    )) {
        unique_tag = tag + std::to_string(++counter);
    }

    object.materials.push_back({
        .tag = unique_tag
    });
}

static auto &LastMaterial(MtlMaterialLoader::Object &object)
{
    if (object.materials.empty()) {
        AddMaterial(object, "default");
    }

    return object.materials.back();
}

LoaderResult MtlMaterialLoader::LoadFn(LoaderState *state, Object &object)
{
    const std::unordered_map<std::string, Material::TextureKey> texture_keys{
        std::make_pair("map_Kd", Material::MATERIAL_TEXTURE_ALBEDO_MAP),
        std::make_pair("map_bump", Material::MATERIAL_TEXTURE_NORMAL_MAP),
        std::make_pair("map_Ks", Material::MATERIAL_TEXTURE_METALNESS_MAP),
        std::make_pair("map_Ns", Material::MATERIAL_TEXTURE_ROUGHNESS_MAP)
    };

    Tokens tokens;
    tokens.reserve(5);

    auto splitter = [&tokens](const std::string &token) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    };

    state->stream.ReadLines([&](const std::string &line) {
        tokens.clear();

        const auto trimmed = StringUtil::Trim(line);

        if (trimmed.empty() || trimmed.front() == '#') {
            return;
        }

        StringUtil::SplitBuffered(trimmed, ' ', splitter);

        if (tokens.empty()) {
            return;
        }

        if (tokens[0] == "newmtl") {
            std::string name = "default";

            if (tokens.size() >= 2) {
                name = tokens[1];
            } else {
                DebugLog(LogType::Warn, "Obj Mtl loader: material arg name missing\n");
            }

            AddMaterial(object, name);

            return;
        }

        if (tokens[0] == "Kd") {
            auto color = ReadVector<Vector4>(tokens);

            if (tokens.size() < 5) {
                color.w = 1.0f;
            }

            LastMaterial(object).parameters[Material::MATERIAL_KEY_ALBEDO] = ParameterDef{
                .values = std::vector<float>(std::begin(color.values), std::end(color.values))
            };

            return;
        }

        if (tokens[0] == "Ns") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: spec value missing\n");

                return;
            }

            const auto spec = StringUtil::Parse<int>(tokens[1]);

            LastMaterial(object).parameters[Material::MATERIAL_KEY_EMISSIVENESS] = ParameterDef{
                .values = {float(spec) / 100.0f}
            };

            return;
        }

        if (tokens[0] == "illum") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: illum value missing\n");

                return;
            }

            const auto illum_model = StringUtil::Parse<int>(tokens[1]);

            LastMaterial(object).parameters[Material::MATERIAL_KEY_METALNESS] = ParameterDef{
                .values = {float(illum_model) / 9.0f} /* rough approx */
            };

            return;
        }

        const auto texture_it = texture_keys.find(tokens[0]);

        if (texture_it != texture_keys.end()) {
            std::string name;

            if (tokens.size() >= 2) {
                name = tokens[1];
            } else {
                DebugLog(LogType::Warn, "Obj Mtl loader: texture arg name missing\n");
            }

            LastMaterial(object).textures.push_back({
                .key = texture_it->second,
                .name = name
            });

            return;
        }

        DebugLog(LogType::Warn, "Unable to parse mtl material line: %s\n", trimmed.c_str());
    });

    return {};
}

std::unique_ptr<MaterialLibrary> MtlMaterialLoader::BuildFn(Engine *engine, const Object &object)
{
    auto material_library = std::make_unique<MaterialLibrary>();

    std::set<std::string> texture_filepaths;

    for (const auto &item : object.materials) {
        for (const auto &it : item.textures) {
            texture_filepaths.insert(it.name);
        }
    }

    std::vector<std::string> all_filepaths;
    all_filepaths.insert(all_filepaths.end(), texture_filepaths.begin(), texture_filepaths.end());

    auto loaded_textures = engine->assets.Load<Texture2D>(std::vector<std::string>(all_filepaths));

    std::unordered_map<std::string, Ref<Texture>> texture_refs;

    for (size_t i = 0; i < loaded_textures.size(); i++) {
        texture_refs[all_filepaths[i]] = engine->resources.textures.Add(std::move(loaded_textures[i]));
    }

    engine->resources.Lock([&](Resources &resources) {
        for (auto &item : object.materials) {
            auto material = std::make_unique<Material>();

            for (auto &it : item.parameters) {
                material->SetParameter(it.first, Material::Parameter(it.second.values.data(), it.second.values.size()));
            }

            for (auto &it : item.textures) {
                /* TODO */
                //material->SetTexture(it.key, texture_refs[it.name].Acquire());
            }

            material_library->Add(item.tag, resources.materials.Add(std::move(material)));
        }
    });

    return std::move(material_library);
}

} // namespace hyperion::v2