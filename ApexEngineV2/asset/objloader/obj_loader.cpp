#include "obj_loader.h"
#include "../../asset/asset_manager.h"
#include "../../rendering/mesh.h"
#include "../../rendering/vertex.h"
#include "../../util/string_util.h"
#include "../../entity.h"
#include "mtl_loader.h"

#include <fstream>

namespace apex {
void ObjModel::AddMesh(const std::string &name)
{
    int counter = 0;
    while (std::find(mesh_names.begin(), mesh_names.end(), name) != mesh_names.end()) {
        ++counter;
    }

    mesh_names.push_back((counter == 0 ? name : name + std::to_string(counter)));
    indices.push_back(std::vector<ObjIndex>());

}

std::vector<ObjModel::ObjIndex> &ObjModel::CurrentList()
{
    if (indices.empty()) {
        AddMesh("mesh");
    }
    return indices.back();
}

ObjModel::ObjIndex ObjModel::ParseObjIndex(const std::string &token)
{
    auto tokens = StringUtil::Split(token, '/');

    ObjIndex res;
    if (!tokens.empty()) {
        res.vertex_idx = std::stoi(tokens[0]) - 1;
        if (tokens.size() > 1) {
            if (!tokens[1].empty()) {
                has_texcoords = true;
                res.texcoord_idx = std::stoi(tokens[1]) - 1;
            }
            if (tokens.size() > 2) {
                if (!tokens[2].empty()) {
                    has_normals = true;
                    res.normal_idx = std::stoi(tokens[2]) - 1;
                }
            }
        }
    }
    return res;
}

std::shared_ptr<Loadable> ObjLoader::LoadFromFile(const std::string &path)
{
    ObjModel model;

    auto res = std::make_shared<Entity>();

    std::string dir(path);
    std::string model_name = dir.substr(dir.find_last_of("\\/") + 1);
    model_name = model_name.substr(0, model_name.find_first_of("."));

    res->SetName(model_name);

    std::string line;
    std::ifstream fs(path);
    if (!fs.is_open()) {
        return nullptr;
    }
    while (std::getline(fs, line)) {
        auto tokens = StringUtil::Split(line, ' ');
        tokens = StringUtil::RemoveEmpty(tokens);

        if (!tokens.empty() && tokens[0] != "#") {
            if (tokens[0] == "v") {
                float x = std::stof(tokens[1]);
                float y = std::stof(tokens[2]);
                float z = std::stof(tokens[3]);
                model.positions.push_back(Vector3(x, y, z));
            } else if (tokens[0] == "vn") {
                float x = std::stof(tokens[1]);
                float y = std::stof(tokens[2]);
                float z = std::stof(tokens[3]);
                model.normals.push_back(Vector3(x, y, z));
            } else if (tokens[0] == "vt") {
                float x = std::stof(tokens[1]);
                float y = std::stof(tokens[2]);
                model.texcoords.push_back(Vector2(x, y));
            } else if (tokens[0] == "f") {
                auto &list = model.CurrentList();
                for (int i = 0; i < tokens.size() - 3; i++) {
                    list.push_back(model.ParseObjIndex(tokens[1]));
                    list.push_back(model.ParseObjIndex(tokens[2 + i]));
                    list.push_back(model.ParseObjIndex(tokens[3 + i]));
                }
            } else if (tokens[0] == "mtllib") {
                std::string loc = tokens[1];
                std::string dir(path);
                dir = dir.substr(0, dir.find_last_of("\\/"));
                if (!(StringUtil::Contains(dir, "/") || StringUtil::Contains(dir, "\\"))) {
                    dir.clear();
                }
                dir += "/" + loc;

                // load material library
                model.mtl_lib = AssetManager::GetInstance()->LoadFromFile<MtlLib>(dir);
            } else if (tokens[0] == "usemtl") {
                model.AddMesh(tokens[1]);
            }
        }
    }

    for (size_t i = 0; i < model.indices.size(); i++) {
        auto &list = model.indices[i];
        std::vector<Vertex> vertices;

        for (auto &idc : list) {
            vertices.push_back(Vertex(
                (model.positions[idc.vertex_idx]),
                (model.has_texcoords ? model.texcoords[idc.texcoord_idx] : Vector2()),
                (model.has_normals ? model.normals[idc.normal_idx] : Vector3())));
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->SetVertices(vertices);

        if (model.has_normals) {
            mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);
        }
        if (model.has_texcoords) {
            mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
        }

        auto geom = std::make_shared<Entity>();
        geom->SetName(model.mesh_names[i]);
        geom->SetRenderable(mesh);
        res->AddChild(geom);
    }

    return res;
}
}