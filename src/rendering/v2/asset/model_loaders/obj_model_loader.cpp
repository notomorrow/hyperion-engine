#include "obj_model_loader.h"
#include <rendering/v2/engine.h>
#include <rendering/v2/components/mesh.h>

#include <algorithm>

namespace hyperion::v2 {

using Tokens = std::vector<std::string>;
using ObjModelLoader = LoaderObject<Node, LoaderFormat::MODEL_OBJ>::Loader;

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
                obj_index.vertex   = std::atoll(index_str.c_str()) - 1;
                break;
            case 1:
                obj_index.texcoord = std::atoll(index_str.c_str()) - 1;
                break;
            case 2:
                obj_index.normal   = std::atoll(index_str.c_str()) - 1;
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

template <class Vector>
Vector GetIndexedVertexProperty(int64_t vertex_index, const std::vector<Vector> &vectors)
{
    const int64_t vertex_absolute = vertex_index >= 0
        ? vertex_index
        : int64_t(vectors.size()) + (vertex_index);

    AssertReturnMsg(
        vertex_absolute >= 0 && vertex_absolute < int64_t(vectors.size()),
        Vector(),
        "Vertex index of %lld is out of bounds (%llu)\n",
        vertex_absolute,
        vectors.size()
    );

    return vectors[vertex_absolute];
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
            object.positions.push_back(ReadVector<Vector3>(tokens, 1));

            return;
        }

        if (tokens[0] == "vn") {
            object.normals.push_back(ReadVector<Vector3>(tokens, 1));

            return;
        }

        if (tokens[0] == "vt") {
            object.texcoords.push_back(ReadVector<Vector2>(tokens, 1));

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

std::unique_ptr<Node> ObjModelLoader::BuildFn(Engine *engine, const Object &object)
{
    auto top = std::make_unique<Node>(object.material_library.c_str()); /* TODO */

    const bool has_vertices  = !object.positions.empty(),
               has_normals   = !object.normals.empty(),
               has_texcoords = !object.texcoords.empty();

    for (auto &obj_mesh : object.meshes) {
        std::vector<Vertex> vertices;
        vertices.reserve(object.positions.size());

        std::vector<Mesh::Index> indices;
        indices.reserve(obj_mesh.indices.size());

        std::map<ObjIndex, Mesh::Index> index_map;

        for (const ObjIndex &obj_index : obj_mesh.indices) {
            const auto it = index_map.find(obj_index);

            if (it != index_map.end()) {
                indices.push_back(it->second);

                continue;
            }

            Vertex vertex;
            const Mesh::Index index = Mesh::Index(vertices.size());

            if (has_vertices) {
                vertex.SetPosition(GetIndexedVertexProperty(obj_index.vertex, object.positions));
            }

            if (has_normals) {
                vertex.SetNormal(GetIndexedVertexProperty(obj_index.normal, object.normals));
            }

            if (has_texcoords) {
                vertex.SetTexCoord0(GetIndexedVertexProperty(obj_index.texcoord, object.texcoords));
            }

            vertices.push_back(vertex);
            indices.push_back(index);

            index_map[obj_index] = index;
        }
            
        auto mesh = engine->GetMeshes().Add();
        mesh->SetVertices(vertices, indices);

        if (!has_normals) {
            mesh->CalculateNormals();
        }

        mesh->CalculateTangents();
        
        auto *spatial = engine->AddSpatial(
            mesh,
            mesh->GetVertexAttributes(),
            Transform(),
            BoundingBox(), /* TODO */
            Material::ID{Material::ID::ValueType{1}} /* TODO */
        );

        auto node = std::make_unique<Node>(obj_mesh.tag.c_str());
        node->SetSpatial(spatial);
        top->AddChild(std::move(node));
    }

    return std::move(top);
}

} // namespace hyperion::v2