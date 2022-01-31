#include "entity_marshal.h"

namespace hyperion {
namespace fbom {

FBOMResult EntityMarshal::Deserialize(FBOMObject *in, FBOMLoadable *&out) const
{
    std::string name;

    if (auto prop = in->GetProperty("name")) {
        if (prop.IsString()) {
            prop.ReadString(name);
        }
    }

    Entity *out_entity = new Entity(name);
    out = out_entity;

    for (auto &node : in->nodes) {
        FBOMLoadable *child = nullptr;

        if (Deserialize(node.get(), child) == FBOM_OK) {
            ex_assert(child != nullptr);

            std::shared_ptr<Entity> child_entity;
            child_entity.reset(dynamic_cast<Entity*>(child));

            out_entity->AddChild(child_entity);
        } else {
            if (child != nullptr) {
                delete child;
            }

            return FBOM_ERR;
        }
    }

    return FBOM_OK;
}

FBOMResult EntityMarshal::Serialize(FBOMLoadable *in, FBOMObject *out) const
{
    auto entity = dynamic_cast<Entity*>(in);

    if (entity == nullptr) {
        return FBOM_ERR;
    }

    out->SetProperty("name", FBOMString(), entity->GetName().size(), (unsigned char*)entity->GetName().data());

    return FBOM_OK;
}

} // namespace fbom
} // namespace hyperion
