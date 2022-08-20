#ifndef HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
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

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Entity &out_object) const override
    {
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
            out_object.SetTransform(transform);
        }

        { // bounding box
            BoundingBox local_aabb = BoundingBox::empty;
            BoundingBox world_aabb = BoundingBox::empty;

            if (auto err = in.GetProperty("local_aabb").ReadStruct(sizeof(BoundingBox), &local_aabb)) {
                return err;
            }

            if (auto err = in.GetProperty("world_aabb").ReadStruct(sizeof(BoundingBox), &world_aabb)) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif