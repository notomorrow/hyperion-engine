#include "obj_model_loader.h"

#include <algorithm>

namespace hyperion::v2 {

using Tokens = std::vector<std::string>;

template <class Vector>
static Vector ReadVector(const Tokens &tokens)
{
    Vector result{0.0f};

    int value_index = 0;

    for (const auto &token : tokens) {
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

static void AddMesh(ObjModelLoader::Object &object, const std::string &tag)
{
    std::string unique_tag(tag);
    int counter = 0;

    while (std::any_of(
        object.meshes.begin(),
        object.meshes.end(),
        [&unique_tag](const auto &mesh) { return mesh.tag == unique_tag; }
    )) {
        unique_tag = tag + std::to_string(++counter);
    }

    object.meshes.push_back({
        .tag = unique_tag,
        .material = tag
    });
}

static auto &LastMesh(ObjModelLoader::Object &object)
{
    if (object.meshes.empty()) {
        AddMesh(object, "default");
    }

    return object.meshes.back();
}

static auto ParseObjIndex(const std::string &token)
{
    ObjModelLoader::Object::ObjIndex obj_index{0, 0, 0};
    size_t token_index = 0;

    StringUtil::SplitBuffered(token, '/', [&token_index, &obj_index](const std::string &index_str) {
        if (!index_str.empty()) {
            switch (token_index) {
            case 0:
                obj_index.vertex   = std::atoi(index_str.c_str()) - 1;
                break;
            case 1:
                obj_index.texcoord = std::atoi(index_str.c_str()) - 1;
                break;
            case 2:
                obj_index.normal   = std::atoi(index_str.c_str()) - 1;
                break;
            default:
                // ??
                break;
            }
        }

        ++token_index;
    });

    return obj_index;
}

LoaderResult ObjModelLoader::LoadFn(LoaderStream *stream, Object &object)
{
    Tokens tokens;
    tokens.reserve(5);

    auto splitter = [&tokens](const std::string &token) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    };

    stream->ReadLines([&](const std::string &line) {
        tokens.clear();

        const auto trimmed = StringUtil::Trim(line);

        if (trimmed.empty() || trimmed.front() == '#') {
            return;
        }

        StringUtil::SplitBuffered(trimmed, ' ', splitter);

        if (tokens.empty()) {
            return;
        }

        if (tokens[0] == "v") {
            object.positions.push_back(ReadVector<Vector3>(tokens));

            return;
        }

        if (tokens[0] == "vn") {
            object.normals.push_back(ReadVector<Vector3>(tokens));

            return;
        }

        if (tokens[0] == "vt") {
            object.texcoords.push_back(ReadVector<Vector2>(tokens));

            return;
        }

        if (tokens[0] == "f") {
            auto &last_mesh = LastMesh(object);

            for (size_t i = 1; i < tokens.size(); i++) {
                last_mesh.indices.push_back(ParseObjIndex(tokens[i]));
            }

            return;
        }

        if (tokens[0] == "mtllib") {
            if (tokens.size() > 1) {
                object.material_library = tokens[1];
            }

            return;
        }

        if (tokens[0] == "usemtl") {
            std::string tag = "default";

            if (tokens.size() > 1) {
                tag = tokens[1];
            }

            AddMesh(object, tag);

            return;
        }

        DebugLog(LogType::Warn, "Unable to parse obj model line: %s\n", trimmed.c_str());
    });

    return {};
}

std::unique_ptr<Node> ObjModelLoader::BuildFn(const Object &object)
{
    /* TODO build Node, Meshes, Spatials */
    return nullptr;
}

} // namespace hyperion::v2