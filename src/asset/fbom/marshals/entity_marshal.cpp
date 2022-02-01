#include "entity_marshal.h"

namespace hyperion {
namespace fbom {

FBOMResult EntityMarshal::Deserialize(FBOMLoader *loader, FBOMObject *in, FBOMDeserialized &out) const
{
    /*std::string name;

    if (auto prop = in->GetProperty("name")) {
        if (prop.IsString()) {
            prop.ReadString(name);
        }
    }

    auto out_entity = std::make_shared<Entity>(name);
    out = out_entity;

    for (auto &node : in->nodes) {
        FBOMDeserialized child;
        FBOMResult loader_result = loader->Deserialize(node.get(), child);

        if (loader_result == FBOMResult::FBOM_OK) {
            ex_assert(child != nullptr);

            if (child->GetLoadableType() == "ENTITY") {
                auto child_entity = std::dynamic_pointer_cast<Entity>(child);
                ex_assert(child_entity != nullptr);

                out_entity->AddChild(child_entity);

                continue;
            }

            if (child->GetLoadableType() == "CONTROL") {
                auto control = std::dynamic_pointer_cast<EntityControl>(child);
                ex_assert(control != nullptr);

                out_entity->AddControl(control);

                continue;
            }

            return FBOMResult(FBOMResult::FBOM_ERR, std::string("Entity does not know how to handle ") + node->decl_type + " subnode");
        } else {
            return loader_result;
        }
    }*/

    return FBOMResult::FBOM_OK;
}

FBOMResult EntityMarshal::Serialize(FBOMLoader *loader, FBOMLoadable *in, FBOMObject *out) const
{
    /*auto entity = dynamic_cast<Entity*>(in);

    if (entity == nullptr) {
        return FBOMResult::FBOM_ERR;
    }

    out->SetProperty("name", FBOMString(), entity->GetName().size(), (unsigned char*)entity->GetName().data());

    for (size_t i = 0; i < entity->NumChildren(); i++) {
        if (auto child = entity->GetChild(i)) {
            FBOMObject *child_object = out->AddChild(child->GetLoadableType());
            FBOMResult loader_result = loader->Serialize(child.get(), child_object);

            if (loader_result != FBOMResult::FBOM_OK) {
                return loader_result;
            }
        }
    }

    for (size_t i = 0; i < entity->NumControls(); i++) {
        if (auto control = entity->GetControl(i)) {
            FBOMObject *control_object = out->AddChild(control->GetLoadableType());
            FBOMResult loader_result = loader->Serialize(control.get(), control_object);

            if (loader_result != FBOMResult::FBOM_OK) {
                return loader_result;
            }
        }
    }*/

    return FBOMResult::FBOM_OK;
}

} // namespace fbom
} // namespace hyperion
