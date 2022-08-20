#ifndef HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/MeshMarshal.hpp>
#include <scene/Entity.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Entity> : public FBOMObjectMarshalerBase<Entity>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("Entity");
    }

    virtual FBOMResult Serialize(const Entity &in_object, FBOMObject &out) const override
    {
        out.SetProperty("transform.translation", FBOMVec3f(), &in_object.GetTransform().GetTranslation());
        out.SetProperty("transform.rotation", FBOMQuaternion(), &in_object.GetTransform().GetRotation());
        out.SetProperty("transform.scale", FBOMVec3f(), &in_object.GetTransform().GetScale());

        out.SetProperty("local_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetLocalAABB());
        out.SetProperty("world_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetWorldAABB());

        if (const auto &mesh = in_object.GetMesh()) {
            out.AddChild(*mesh);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Entity *&out_object) const override
    {
        out_object = new Entity();

        { // transform
            Transform transform = Transform::identity;

            if (auto err = in.GetProperty("transform.translation").ReadArrayElements(FBOMFloat(), 3, &transform.GetTranslation())) {
                return err;
            }
        
            if (auto err = in.GetProperty("transform.rotation").ReadArrayElements(FBOMFloat(), 4, &transform.GetRotation())) {
                return err;
            }

            if (auto err = in.GetProperty("transform.scale").ReadArrayElements(FBOMFloat(), 3, &transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            out_object->SetTransform(transform);
        }

        { // bounding box
            BoundingBox local_aabb = BoundingBox::empty;
            BoundingBox world_aabb = BoundingBox::empty;

            if (auto err = in.GetProperty("local_aabb").ReadStruct(sizeof(BoundingBox), &local_aabb)) {
                return err;
            }

            out_object->SetLocalAABB(local_aabb);

            if (auto err = in.GetProperty("world_aabb").ReadStruct(sizeof(BoundingBox), &world_aabb)) {
                return err;
            }

            out_object->SetWorldAABB(world_aabb);
        }

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Mesh")) {
                auto *mesh = node.deserialized.Release<Mesh>();

                HYP_BREAKPOINT;

                delete mesh;
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif