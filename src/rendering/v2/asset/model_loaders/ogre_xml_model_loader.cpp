#include "ogre_xml_model_loader.h"
#include <rendering/v2/engine.h>

#include <util/xml/sax_parser.h>

#include <algorithm>

namespace hyperion::v2 {

using OgreXmlModelLoader = LoaderObject<Node, LoaderFormat::MODEL_OGRE_XML>::Loader;

class OgreXmlSaxHandler : public xml::SaxHandler {
public:
    OgreXmlSaxHandler(OgreXmlModelLoader::Object &object)
        : m_object(object)
    {
    }

    OgreXmlModelLoader::Object::OgreSubMesh &LastSubMesh()
    {
        if (m_object.submeshes.empty()) {
            m_object.submeshes.emplace_back();
        }

        return m_object.submeshes.back();
    }

    void AddBoneAssignment(size_t vertex_index, OgreXmlModelLoader::Object::BoneAssignment &&bone_assignment)
    {
        m_object.bone_assignments[vertex_index].push_back(std::move(bone_assignment));
    }

    virtual void Begin(const std::string &name, const xml::AttributeMap &attributes) override
    {
        if (name == "position") {
            float x = StringUtil::Parse<float>(attributes.at("x"));
            float y = StringUtil::Parse<float>(attributes.at("y"));
            float z = StringUtil::Parse<float>(attributes.at("z"));

            m_object.positions.emplace_back(x, y, z);
        } else if (name == "normal") {
            float x = StringUtil::Parse<float>(attributes.at("x"));
            float y = StringUtil::Parse<float>(attributes.at("y"));
            float z = StringUtil::Parse<float>(attributes.at("z"));

            m_object.normals.emplace_back(x, y, z);
        } else if (name == "texcoord") {
            float x = StringUtil::Parse<float>(attributes.at("u"));
            float y = StringUtil::Parse<float>(attributes.at("v"));

            m_object.texcoords.emplace_back(x, y);
        } else if (name == "face") {
            if (attributes.size() != 3) {
                DebugLog(LogType::Warn, "Ogre XML parser: `face` tag expected to have 3 attributes.\n");
            }

            for (auto &it : attributes) {
                LastSubMesh().indices.push_back(StringUtil::Parse<Mesh::Index>(it.second));
            }
        } else if (name == "skeletonlink") {
            
        } else if (name == "vertexboneassignment") {
            const auto vertex_index = StringUtil::Parse<uint32_t>(attributes.at("vertexindex"));
            const auto bone_index = StringUtil::Parse<uint32_t>(attributes.at("boneindex"));
            const auto bone_weight = StringUtil::Parse<float>(attributes.at("weight"));

            AddBoneAssignment(vertex_index, {bone_index, bone_weight});
        } else if (name == "submesh") {
            m_object.submeshes.emplace_back();
        } else if (name == "vertex") {
            /* no-op */
        }  else {
            DebugLog(LogType::Warn, "Ogre XML parser: No handler for '%s' tag\n", name.c_str());
        }
    }

    virtual void End(const std::string &name) override {}
    virtual void Characters(const std::string &value) override {}
    virtual void Comment(const std::string &comment) override {}

private:
    OgreXmlModelLoader::Object &m_object;
};

void BuildVertices(OgreXmlModelLoader::Object &object)
{
    const bool has_normals   = !object.normals.empty(),
               has_texcoords = !object.texcoords.empty();

    std::vector<Vertex> vertices;
    vertices.resize(object.positions.size());

    for (size_t i = 0; i < vertices.size(); i++) {
        Vector3 position;
        Vector3 normal;
        Vector2 texcoord;

        if (i < object.positions.size()) {
            position = object.positions[i];
        } else {
            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Vertex index (%lu) out of bounds (%llu)\n",
                i,
                object.positions.size()
            );

            continue;
        }

        if (has_normals) {
            if (i < object.normals.size()) {
                normal = object.normals[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Normal index (%lu) out of bounds (%llu)\n",
                    i,
                    object.normals.size()
                );
            }
        }

        if (has_texcoords) {
            if (i < object.texcoords.size()) {
                texcoord = object.texcoords[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Texcoord index (%lu) out of bounds (%llu)\n",
                    i,
                    object.texcoords.size()
                );
            }
        }

        vertices[i] = Vertex(position, texcoord, normal);

        const auto bone_it = object.bone_assignments.find(i);

        if (bone_it != object.bone_assignments.end()) {
            auto &bone_assignments = bone_it->second;

            for (size_t j = 0; j < bone_assignments.size(); j++) {
                if (j == 4) {
                    DebugLog(LogType::Warn, "Ogre XML parser: Attempt to add more than 4 bone assignments\n");

                    break;
                }

                vertices[i].AddBoneIndex(bone_assignments[j].index);
                vertices[i].AddBoneWeight(bone_assignments[j].weight);
            }
        }
    }

    object.vertices = std::move(vertices);
}

LoaderResult OgreXmlModelLoader::LoadFn(LoaderStream *stream, Object &object)
{
    OgreXmlSaxHandler handler(object);

    xml::SaxParser parser(&handler);
    auto sax_result = parser.Parse(stream);

    if (!sax_result) {
        return {LoaderResult::Status::ERR, sax_result.message};
    }
    
    BuildVertices(object);

    return {};
}

std::unique_ptr<Node> OgreXmlModelLoader::BuildFn(Engine *engine, const Object &object)
{
    auto top = std::make_unique<Node>();

    engine->resources.Lock([&](Resources &resources) {
        for (auto &sub_mesh : object.submeshes) {
            if (sub_mesh.indices.empty()) {
                DebugLog(LogType::Warn, "Ogre XML parser: Skipping submesh with empty indices\n");

                continue;
            }

            auto mesh = resources.meshes.Add(
                std::make_unique<Mesh>(
                    object.vertices, 
                    sub_mesh.indices
                )
            );

            if (object.normals.empty()) {
                mesh->CalculateNormals();
            }

            mesh->CalculateTangents();

            auto vertex_attributes = mesh->GetVertexAttributes();

            auto spatial = resources.spatials.Add(
                std::make_unique<Spatial>(
                    std::move(mesh),
                    vertex_attributes,
                    Transform(),
                    BoundingBox(),
                    engine->resources.materials.Get(Material::ID{Material::ID::ValueType{1}})
                )
            );
            
            auto node = std::make_unique<Node>();

            node->SetSpatial(std::move(spatial));

            top->AddChild(std::move(node));
        }
    });

    return std::move(top);
}

} // namespace hyperion::v2