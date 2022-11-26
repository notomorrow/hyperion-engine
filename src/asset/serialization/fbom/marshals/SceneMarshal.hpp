#ifndef HYPERION_V2_FBOM_MARSHALS_SCENE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SCENE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <asset/serialization/fbom/marshals/CameraMarshal.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Scene> : public FBOMObjectMarshalerBase<Scene>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Scene::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Scene &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMString(), in_object.GetName().Size(), in_object.GetName().Data());

        // for (auto &node : in_object.GetRoot().GetChildren()) {
        //     out.AddChild(*node.Get());
        // }

        if (in_object.GetRoot()) {
            out.AddChild(*in_object.GetRoot().Get());
        }

        if (auto *camera = in_object.GetCamera().Get()) {
            out.AddChild(*camera);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto scene_handle = UniquePtr<Handle<Scene>>::Construct(CreateObject<Scene>(Handle<Camera>()));

        String name;
        in.GetProperty("name").ReadString(name);
        (*scene_handle)->SetName(std::move(name));

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Node")) {
                (*scene_handle)->GetRoot().AddChild(node.deserialized.Get<Node>());
                // out_object->SetRoot(node.deserialized.Get<Node>());
            } else if (node.GetType().IsOrExtends(Camera::GetClass().GetName())) {
                (*scene_handle)->SetCamera(node.deserialized.Get<Camera>());
            }
        }

        out_object = std::move(scene_handle);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif