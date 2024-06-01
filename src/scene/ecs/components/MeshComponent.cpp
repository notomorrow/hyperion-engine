#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>

namespace hyperion {

template <>
class ComponentInterface<MeshComponent> : public ComponentInterfaceBase
{
public:
    virtual ~ComponentInterface() override = default;

protected:
    virtual TypeID GetTypeID_Internal() const override
    {
        return TypeID::ForType<MeshComponent>();
    }

    virtual Array<ComponentProperty> GetProperties_Internal() const override
    {
        return {
            {
                HYP_NAME(Mesh),
                [](const void *component, fbom::FBOMData *out)
                {
                    DebugLog(LogType::Debug, "Mesh ID = #%u\n", static_cast<const MeshComponent *>(component)->mesh.GetID().Value());
                    *out = fbom::FBOMData::FromUnsignedInt(static_cast<const MeshComponent *>(component)->mesh.GetID().Value());
                }
            }
        };
    }
};

HYP_REGISTER_COMPONENT_INTERFACE(MeshComponent);

} // namespace hyperion