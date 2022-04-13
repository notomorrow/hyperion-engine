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
    object.filepath = state->filepath;

    const std::unordered_map<std::string, Material::TextureKey> texture_keys{
        std::make_pair("map_kd",   Material::MATERIAL_TEXTURE_ALBEDO_MAP),
        std::make_pair("map_bump", Material::MATERIAL_TEXTURE_NORMAL_MAP),
        std::make_pair("map_ka",   Material::MATERIAL_TEXTURE_METALNESS_MAP),
        std::make_pair("map_ks",   Material::MATERIAL_TEXTURE_METALNESS_MAP),
        std::make_pair("map_ns",   Material::MATERIAL_TEXTURE_ROUGHNESS_MAP)
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

        tokens[0] = StringUtil::ToLower(tokens[0]);

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

        if (tokens[0] == "kd") {
            auto color = ReadVector<Vector4>(tokens);

            if (tokens.size() < 5) {
                color.w = 1.0f;
            }

            LastMaterial(object).parameters[Material::MATERIAL_KEY_ALBEDO] = ParameterDef{
                .values = std::vector<float>(std::begin(color.values), std::end(color.values))
            };

            return;
        }

        if (tokens[0] == "ns") {
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

        if (tokens[0] == "bump") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: bump value missing\n");

                return;
            }

            std::string bump_name = tokens[tokens.size() - 1];
            
            LastMaterial(object).textures.push_back({
                .key = Material::MATERIAL_TEXTURE_NORMAL_MAP,
                .name = bump_name
            });

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

        DebugLog(LogType::Warn, "Obj Mtl loader: Unable to parse mtl material line: %s\n", trimmed.c_str());
    });

    return {};
}

std::unique_ptr<MaterialLibrary> MtlMaterialLoader::BuildFn(Engine *engine, const Object &object)
{
    auto material_library = std::make_unique<MaterialLibrary>();
    
    std::unordered_map<std::string, std::string> texture_names_to_path;

    for (const auto &item : object.materials) {
        for (const auto &it : item.textures) {
            const auto texture_path = StringUtil::BasePath(object.filepath) + "/" + it.name;

            texture_names_to_path[it.name] = texture_path;
        }
    }
    
    std::vector<std::string> all_filepaths;
    std::vector<std::unique_ptr<Texture>> loaded_textures;

    if (!texture_names_to_path.empty()) {
        for (auto &it : texture_names_to_path) {
            all_filepaths.push_back(it.second);
        }

        loaded_textures = engine->assets.Load<Texture>(all_filepaths);
    }

    engine->resources.Lock([&](Resources &resources) {
        std::unordered_map<std::string, Ref<Texture>> texture_refs;

        for (size_t i = 0; i < loaded_textures.size(); i++) {
            if (loaded_textures[i] == nullptr) {
                texture_refs[all_filepaths[i]] = nullptr;

                continue;
            }

            texture_refs[all_filepaths[i]] = engine->resources.textures.Add(std::move(loaded_textures[i]));
        }

        for (auto &item : object.materials) {
            auto material = std::make_unique<Material>(item.tag.c_str());

            for (auto &it : item.parameters) {
                material->SetParameter(it.first, Material::Parameter(it.second.values.data(), it.second.values.size()));
            }

            for (auto &it : item.textures) {
                if (texture_refs[texture_names_to_path[it.name]] == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "Obj Mtl loader: Texture %s could not be used because it could not be loaded\n",
                        it.name.c_str()
                    );

                    continue;
                }
                
                material->SetTexture(it.key, texture_refs[texture_names_to_path[it.name]].Acquire());
            }

            material_library->Add(item.tag, resources.materials.Add(std::move(material)));
        }
    });

    return std::move(material_library);
}

} // namespace hyperion::v2