#include "obj_loader.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../rendering/environment.h"
#include "../../rendering/mesh.h"
#include "../../math/vertex.h"
#include "../../util/string_util.h"
#include "../../entity.h"
#include "../../asset/buffered_text_reader.h"
#include "mtl_loader.h"

#include <fstream>
#include <set>

namespace hyperion {
const bool ObjLoader::use_indices = true;

void ObjModel::AddMesh(const std::string &name)
{
    std::string mesh_name = name;

    int counter = 0;

    while (std::find_if(meshes.begin(), meshes.end(), [mesh_name](auto &mesh) { return mesh.name == mesh_name; }) != meshes.end()) {
        ++counter;
        mesh_name = name + std::to_string(counter);
    }

    meshes.push_back(ObjMesh{ mesh_name, name });
}

ObjMesh &ObjModel::CurrentList()
{
    if (meshes.empty()) {
        AddMesh("mesh");
    }

    return meshes.back();
}

ObjIndex ObjModel::ParseObjIndex(const std::string &token)
{
    auto tokens = StringUtil::Split(token, '/');

    ObjIndex res;
    res.vertex_idx = 0;
    res.normal_idx = 0;
    res.texcoord_idx = 0;

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
    /*auto loaded_text = TextLoader().LoadTextFromFile(path);

    if (loaded_text == nullptr) {
        return nullptr;
    }*/

    ObjModel model;

    auto res = std::make_shared<Entity>();

    // model name
    std::string dir(path);
    std::string model_name = dir.substr(dir.find_last_of("\\/") + 1);
    model_name = model_name.substr(0, model_name.find_first_of("."));

    res->SetName(model_name);



    //std::vector<std::string> lines = StringUtil::Split(loaded_text->GetText(), '\n');
  
    int line_no = 0;

    BufferedTextReader<2048> buf(path);

    std::vector<std::string> tokens;
    tokens.reserve(5);

    auto split_tokens = [&, this](std::string token) {
        if (token.length() != 0) {
            tokens.push_back(token);
        }
    };

    //for (auto &line : lines) {
    buf.ReadLines([&, this](auto line) {
        tokens.clear();

        StringUtil::SplitBuffered(line, ' ', split_tokens);

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
                auto &mesh = model.CurrentList();

                for (int i = 0; i < tokens.size() - 3; i++) {
                    mesh.indices.push_back(model.ParseObjIndex(tokens[1]));
                    mesh.indices.push_back(model.ParseObjIndex(tokens[2 + i]));
                    mesh.indices.push_back(model.ParseObjIndex(tokens[3 + i]));
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
    });

    for (size_t i = 0; i < model.meshes.size(); i++) {
        auto &obj_mesh = model.meshes[i];

        std::vector<Vertex> mesh_vertices;
        mesh_vertices.reserve(obj_mesh.indices.size());

        std::vector<MeshIndex> mesh_indices;
        mesh_indices.reserve(obj_mesh.indices.size());

        std::map<ObjIndex, MeshIndex> m_index_map; // map objindex to in hyperion mesh index

        for (auto &idc : obj_mesh.indices) {
            auto it = m_index_map.find(idc);

            if (it != m_index_map.end()) {
                mesh_indices.push_back(it->second);
                continue;
            }

            Vertex vertex;
            MeshIndex mesh_index = mesh_vertices.size();

            if (idc.vertex_idx >= 0) {
                ex_assert(idc.vertex_idx < model.positions.size());
                vertex.SetPosition(model.positions[idc.vertex_idx]);
            } else {
                ex_assert(idc.vertex_idx * -1 < model.positions.size());
                vertex.SetPosition(model.positions[model.positions.size() + idc.vertex_idx]);
            }

            if (model.has_normals) {
                if (idc.normal_idx >= 0) {
                    ex_assert(idc.normal_idx < model.normals.size());
                    vertex.SetNormal(model.normals[idc.normal_idx]);
                } else {
                    ex_assert(idc.normal_idx * -1 < model.normals.size());
                    vertex.SetPosition(model.normals[model.normals.size() + idc.normal_idx]);
                }
            }

            if (model.has_texcoords) {
                if (idc.texcoord_idx >= 0) {
                    ex_assert(idc.texcoord_idx < model.texcoords.size());
                    vertex.SetTexCoord0(model.texcoords[idc.texcoord_idx]);
                } else {
                    ex_assert(idc.texcoord_idx * -1 < model.texcoords.size());
                    vertex.SetTexCoord0(model.texcoords[model.texcoords.size() + idc.texcoord_idx]);
                }
            }

            mesh_vertices.push_back(vertex);
            mesh_indices.push_back(mesh_index);
            m_index_map[idc] = mesh_index;
        }


        auto mesh = std::make_shared<Mesh>();
        mesh->SetVertices(mesh_vertices, mesh_indices);

        if (model.has_normals) {
            mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);

            mesh->CalculateTangents();
        } else {
            mesh->CalculateNormals();
        }

        if (model.has_texcoords) {
            mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
        }

        mesh->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()
            .Define("NORMAL_MAPPING", true)
        ));

        auto geom = std::make_shared<Entity>();
        geom->SetName(obj_mesh.name);
        geom->SetRenderable(mesh);

        if (model.mtl_lib) {
            if (auto material_ptr = model.mtl_lib->GetMaterial(obj_mesh.mtl)) {
                geom->SetMaterial(*material_ptr);
            }
        }

        res->AddChild(geom);
    }

    return res;
}
}
