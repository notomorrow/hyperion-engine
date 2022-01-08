#include "obj_loader.h"
#include "../../asset/asset_manager.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../rendering/mesh.h"
#include "../../rendering/vertex.h"
#include "../../util/string_util.h"
#include "../../entity.h"
#include "mtl_loader.h"

#include <fstream>

namespace apex {
void ObjModel::AddMesh(const std::string &name)
{
    std::string mesh_name = name;

    int counter = 0;

    while (std::find(mesh_names.begin(), mesh_names.end(), mesh_name) != mesh_names.end()) {
        ++counter;
        mesh_name = name + std::to_string(counter);
    }

    mesh_names.push_back(mesh_name);
    mesh_material_names[mesh_name] = name;

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
            } else {
                res.texcoord_idx = -1;
            }
            if (tokens.size() > 2) {
                if (!tokens[2].empty()) {
                    has_normals = true;
                    res.normal_idx = std::stoi(tokens[2]) - 1;
                } else {
                    res.normal_idx = -1;
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

    int line_no = 0;

    while (std::getline(fs, line)) {
        line = StringUtil::Trim(line);
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

                model.mtl_lib = AssetManager::GetInstance()->LoadFromFile<MtlLib>(dir);
            } else if (tokens[0] == "usemtl") {
                model.AddMesh(tokens[1]);
            }
        }

        line_no++;
    }

    for (size_t i = 0; i < model.indices.size(); i++) {
        auto &list = model.indices[i];
        std::vector<Vertex> vertices;

        for (auto &idc : list) {
            Vertex vertex;

            if (idc.vertex_idx >= 0) {
                vertex.SetPosition(model.positions[idc.vertex_idx]);
            }

            if (model.has_normals && idc.normal_idx >= 0) {
                vertex.SetNormal(model.normals.at(idc.normal_idx));
            }

            if (model.has_texcoords && idc.texcoord_idx >= 0) {
                vertex.SetTexCoord0(model.texcoords.at(idc.texcoord_idx));
            }

            vertices.push_back(vertex);
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->SetVertices(vertices);

        if (model.has_normals) {
            mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);

            mesh->CalculateTangents();
        } else {
            mesh->CalculateNormals();
        }

        if (model.has_texcoords) {
            mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
        }

        mesh->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
            { "NORMAL_MAPPING", 1 }
        }));

        auto geom = std::make_shared<Entity>();
        geom->SetName(model.mesh_names[i]);
        geom->SetRenderable(mesh);

        if (model.mtl_lib) {
            auto it = model.mesh_material_names.find(model.mesh_names[i]);

            if (it != model.mesh_material_names.end()) {
                if (auto material_ptr = model.mtl_lib->GetMaterial(it->second)) {
                    geom->SetMaterial(*material_ptr);
                }
            }
        }

        res->AddChild(geom);
    }

    return res;
}
}
