#include "apx_loader.h"
#include "../../animation/bone.h"

namespace hyperion {
ApxModel::ApxModel()
    : n_faces_per_vertex(0), last_entity(nullptr)
{
}

void ApxModel::BuildModel()
{
}

std::shared_ptr<Loadable> ApxLoader::LoadFromFile(const std::string &path)
{
    ApxModel model;

    ByteReader *reader = new FileByteReader(path);

    while (reader->Position() < reader->Max()) {
        int32_t ins;
        reader->Read(&ins, sizeof(int32_t));
        Handle(model, reader, (ApxCommand)ins);
    }

    std::string current_dir(path);
    std::string name = current_dir.substr(current_dir.find_last_of("\\/") + 1);
    name = name.substr(0, name.find_first_of("."));

    delete reader;

    if (model.entities.empty()) {
        return nullptr;
    }

    return model.entities[0];
}

void ApxLoader::Read(ByteReader *reader, ApxCommand expected)
{
}

void ApxLoader::Handle(ApxModel &model, ByteReader *&reader, ApxCommand cmd)
{
    using std::string;

    switch (cmd) {
    case CMD_NODE:
    {
        int32_t length;
        reader->Read(&length);

        char *name = new char[length];
        reader->Read(name, length);

        auto entity = std::make_shared<Entity>();
        entity->SetName(name);

        delete[] name;

        if (model.last_entity) {
            model.last_entity->AddChild(entity);
        }

        model.last_entity = entity.get();
        model.entities.push_back(entity);

        break;
    }
    case CMD_MATERIAL:
    {
        model.materials.push_back(Material());
        break;
    }
    case CMD_MATERIAL_PROPERTY:
    {
        Material &last = model.materials.back();

        int32_t length;
        reader->Read(&length);

        char *name = new char[length];
        reader->Read(&name, length);

        int32_t type;
        reader->Read(&type);

        if (type == CMD_TYPE_INT) {
            int32_t val;
            reader->Read(&val);
            last.SetParameter(name, (int)val);
        } else if (type == CMD_TYPE_FLOAT) {
            float val;
            reader->Read(&val);
            last.SetParameter(name, val);
        } else if (type == CMD_TYPE_VECTOR2) {
            float x, y;
            reader->Read(&x);
            reader->Read(&y);
            last.SetParameter(name, Vector2(x, y));
        } else if (type == CMD_TYPE_VECTOR3) {
            float x, y, z;
            reader->Read(&x);
            reader->Read(&y);
            reader->Read(&z);
            last.SetParameter(name, Vector3(x, y, z));
        } else if (type == CMD_TYPE_VECTOR4) {
            float x, y, z, w;
            reader->Read(&x);
            reader->Read(&y);
            reader->Read(&z);
            reader->Read(&w);
            last.SetParameter(name, Vector4(x, y, z, w));
        } else {
            throw "type not implemented";
        }

        delete[] name;
        break;
    }
    case CMD_TRANSLATION:
    {
        float x, y, z;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);
        model.entities.back()->SetLocalTranslation(Vector3(x, y, z));
        break;
    }
    case CMD_SCALE:
    {
        float x, y, z;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);
        model.entities.back()->SetLocalScale(Vector3(x, y, z));
        break;
    }
    case CMD_ROTATION:
    {
        float x, y, z, w;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);
        reader->Read(&w);
        model.entities.back()->SetLocalRotation(Quaternion(x, y, z, w));
        break;
    }
    case CMD_MESH:
    {
        model.meshes.push_back(std::make_shared<Mesh>());
        break;
    }
    case CMD_VERTICES:
    {
        model.vertices.push_back({});
        model.positions.push_back({});
        model.normals.push_back({});
        model.texcoords0.push_back({});
        model.texcoords1.push_back({});
        break;
    }
    case CMD_POSITION:
    {
        float x, y, z;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);
        model.positions.back().push_back(Vector3(x, y, z));
        break;
    }
    case CMD_NORMAL:
    {
        float x, y, z;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);
        model.normals.back().push_back(Vector3(x, y, z));
        break;
    }
    case CMD_TEXCOORD0:
    {
        float x, y;
        reader->Read(&x);
        reader->Read(&y);
        model.texcoords0.back().push_back(Vector2(x, y));
        break;
    }
    case CMD_TEXCOORD1:
    {
        float x, y;
        reader->Read(&x);
        reader->Read(&y);
        model.texcoords1.back().push_back(Vector2(x, y));
        break;
    }
    case CMD_FACES:
    {
        reader->Read(&model.n_faces_per_vertex);
        model.faces.push_back({});
        break;
    }
    case CMD_FACE:
    {
        auto &list = model.faces.back();

        if (model.n_faces_per_vertex == 0) {
            throw "faces not read correctly";
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < model.n_faces_per_vertex; j++) {
                int32_t val;
                reader->Read(&val);
                list.push_back(val);
            }
        }

        break;
    }
    case CMD_BONE:
    {
        int32_t length;
        reader->Read(&length);

        char *name = new char[length];
        reader->Read(name, length);

        auto bone = std::make_shared<Bone>();
        bone->SetName(name);

        delete[] name;

        if (model.last_entity) {
            model.last_entity->AddChild(bone);
        }

        model.last_entity = bone.get();
        model.bones.push_back(bone);

        break;
    }
    case CMD_BONE_BINDPOSITION:
    {
        float x, y, z;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);

        if (model.last_entity) {
            Bone *ptr = dynamic_cast<Bone*>(model.last_entity);
            if (ptr) {
                ptr->bind_pos = Vector3(x, y, z);
            }
        }

        break;
    }
    case CMD_BONE_BINDROTATION:
    {
        float x, y, z, w;
        reader->Read(&x);
        reader->Read(&y);
        reader->Read(&z);
        reader->Read(&w);

        if (model.last_entity) {
            Bone *ptr = dynamic_cast<Bone*>(model.last_entity);
            if (ptr) {
                ptr->bind_rot = Quaternion(x, y, z, w);
            }
        }

        break;
    }
    // TODO
    }
}
}
