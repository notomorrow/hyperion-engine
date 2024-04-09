#include <asset/model_loaders/OgreXMLModelLoader.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/AnimationComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <Engine.hpp>

#include <util/xml/SAXParser.hpp>

#include <algorithm>

namespace hyperion::v2 {

using OgreXMLModel = OgreXMLModelLoader::OgreXMLModel;
using BoneAssignment = OgreXMLModelLoader::OgreXMLModel::BoneAssignment;
using SubMesh = OgreXMLModelLoader::OgreXMLModel::SubMesh;

class OgreXMLSAXHandler : public xml::SAXHandler
{
public:
    OgreXMLSAXHandler(LoaderState *state, OgreXMLModel &object)
        : m_model(object)
    {
    }

    SubMesh &LastSubMesh()
    {
        if (m_model.submeshes.Empty()) {
            m_model.submeshes.PushBack({ });
        }

        return m_model.submeshes.Back();
    }

    void AddBoneAssignment(uint vertex_index, BoneAssignment &&bone_assignment)
    {
        m_model.bone_assignments[vertex_index].PushBack(std::move(bone_assignment));
    }

    virtual void Begin(const String &name, const xml::AttributeMap &attributes) override
    {
        if (name == "position") {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z")) {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("x"));
            const auto y = StringUtil::Parse<float>(attributes.At("y"));
            const auto z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.positions.PushBack(Vector3(x, y, z));
        } else if (name == "normal") {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z")) {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("x"));
            const auto y = StringUtil::Parse<float>(attributes.At("y"));
            const auto z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.normals.PushBack(Vector3(x, y, z));
        } else if (name == "texcoord") {
            if (!attributes.Contains("u") || !attributes.Contains("v")) {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("u"));
            const auto y = StringUtil::Parse<float>(attributes.At("v"));

            m_model.texcoords.PushBack(Vector2(x, y));
        } else if (name == "face") {
            if (attributes.Size() != 3) {
                DebugLog(LogType::Warn, "Ogre XML parser: `face` tag expected to have 3 attributes.\n");
            }

            for (auto &it : attributes) {
                LastSubMesh().indices.PushBack(StringUtil::Parse<Mesh::Index>(it.second));
            }
        } else if (name == "skeletonlink") {
            m_model.skeleton_name = attributes.At("name");
        } else if (name == "vertexboneassignment") {
            const auto vertex_index = StringUtil::Parse<uint32>(attributes.At("vertexindex"));
            const auto bone_index   = StringUtil::Parse<uint32>(attributes.At("boneindex"));
            const auto bone_weight  = StringUtil::Parse<float>(attributes.At("weight"));

            AddBoneAssignment(vertex_index, {bone_index, bone_weight});
        } else if (name == "submesh") {
            m_model.submeshes.PushBack({ });
        } else if (name == "vertex") {
            /* no-op */
        }  else {
            DebugLog(LogType::Warn, "Ogre XML parser: No handler for '%s' tag\n", name.Data());
        }
    }

    virtual void End(const String &name) override {}
    virtual void Characters(const String &value) override {}
    virtual void Comment(const String &comment) override {}

private:
    OgreXMLModel &m_model;
};

void BuildVertices(OgreXMLModel &model)
{
    const bool has_normals = !model.normals.Empty(),
        has_texcoords = !model.texcoords.Empty();

    Array<Vertex> vertices;
    vertices.Resize(model.positions.Size());

    for (uint i = 0; i < vertices.Size(); i++) {
        Vec3f position;
        Vec3f normal;
        Vec2f texcoord;

        if (i < model.positions.Size()) {
            position = model.positions[i];
        } else {
            DebugLog(
                LogType::Warn,
                "Ogre XML parser: Vertex index (%llu) out of bounds (%llu)\n",
                i,
                model.positions.Size()
            );

            continue;
        }

        if (has_normals) {
            if (i < model.normals.Size()) {
                normal = model.normals[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Normal index (%lu) out of bounds (%llu)\n",
                    i,
                    model.normals.Size()
                );
            }
        }

        if (has_texcoords) {
            if (i < model.texcoords.Size()) {
                texcoord = model.texcoords[i];
            } else {
                DebugLog(
                    LogType::Warn,
                    "Ogre XML parser: Texcoord index (%lu) out of bounds (%llu)\n",
                    i,
                    model.texcoords.Size()
                );
            }
        }

        vertices[i] = Vertex(position, texcoord, normal);

        const auto bone_it = model.bone_assignments.Find(i);

        if (bone_it != model.bone_assignments.end()) {
            auto &bone_assignments = bone_it->second;

            for (SizeType j = 0; j < bone_assignments.Size(); j++) {
                if (j == 4) {
                    DebugLog(LogType::Warn, "Ogre XML parser: Attempt to add more than 4 bone assignments\n");

                    break;
                }

                vertices[i].AddBoneIndex(bone_assignments[j].index);
                vertices[i].AddBoneWeight(bone_assignments[j].weight);
            }
        }
    }

    model.vertices = std::move(vertices);
}

LoadedAsset OgreXMLModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    OgreXMLModel model;
    model.filepath = state.filepath;

    OgreXMLSAXHandler handler(&state, model);

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        return { { LoaderResult::Status::ERR, sax_result.message }, UniquePtr<void>() };
    }
    
    BuildVertices(model);

    auto top = UniquePtr<Node>::Construct();

    Handle<Skeleton> skeleton;

    if (!model.skeleton_name.Empty()) {
        const String skeleton_path((StringUtil::BasePath(model.filepath.Data()).c_str() + String("/") + model.skeleton_name + ".xml").Data());

        LoaderResult result;
        skeleton = state.asset_manager->Load<Skeleton>(skeleton_path, result);

        if (result.status != LoaderResult::Status::OK || !skeleton) {
            DebugLog(LogType::Warn, "Ogre XML parser: Could not load skeleton at %s\n", skeleton_path.Data());
        }
    }

    for (auto &sub_mesh : model.submeshes) {
        if (sub_mesh.indices.Empty()) {
            DebugLog(LogType::Info, "Ogre XML parser: Skipping submesh with empty indices\n");

            continue;
        }

        const Handle<Scene> &scene = g_engine->GetWorld()->GetDetachedScene(Threads::CurrentThreadID());

        const ID<Entity> entity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent(
            entity,
            TransformComponent { }
        );

        scene->GetEntityManager()->AddComponent(
            entity,
            VisibilityStateComponent { }
        );

        Handle<Mesh> mesh = CreateObject<Mesh>(
            model.vertices,
            sub_mesh.indices,
            Topology::TRIANGLES
        );

        if (model.normals.Empty()) {
            mesh->CalculateNormals();
        }

        mesh->CalculateTangents();
        InitObject(mesh);

        auto vertex_attributes = mesh->GetVertexAttributes();
        
        ShaderProperties shader_properties(vertex_attributes);

        Handle<Material> material = CreateObject<Material>(HYP_NAME(ogrexml_material));
        material->SetShader(g_shader_manager->GetOrCreate(HYP_NAME(Forward), shader_properties));
        InitObject(material);

        scene->GetEntityManager()->AddComponent(
            entity,
            MeshComponent {
                mesh,
                material,
                skeleton
            }
        );

        scene->GetEntityManager()->AddComponent(
            entity,
            BoundingBoxComponent {
                mesh->GetAABB()
            }
        );

        if (skeleton.IsValid()) {
            InitObject(skeleton);

            scene->GetEntityManager()->AddComponent(
                entity,
                AnimationComponent {
                    // {
                    //     .animation_index = 0,
                    //     .status = AnimationPlaybackStatus::ANIMATION_PLAYBACK_STATUS_PLAYING,
                    //     .loop_mode = AnimationLoopMode::ANIMATION_LOOP_MODE_REPEAT,
                    //     .speed = 1.0f,
                    //     .current_time = 0.0f
                    // }
                }
            );
        }
        
        NodeProxy node(new Node);
        node.SetEntity(entity);

        top->AddChild(std::move(node));
    }

    return { { LoaderResult::Status::OK }, top.Cast<void>() };
}

} // namespace hyperion::v2
