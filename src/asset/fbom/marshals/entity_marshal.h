#ifndef ENTITY_MARSHAL_H
#define ENTITY_MARSHAL_H

#include "../fbom.h"
#include "../../../entity.h"

namespace hyperion {
namespace fbom {

class EntityMarshal : public Marshal {
public:
    EntityMarshal() = default;
    virtual ~EntityMarshal() = default;

    virtual FBOMResult Deserialize(FBOMObject *in, FBOMLoadable *&out) const override;
    virtual FBOMResult Serialize(FBOMLoadable *in, FBOMObject *out) const override;
};

} // namespace fbom
} // namespace hyperion

#endif
