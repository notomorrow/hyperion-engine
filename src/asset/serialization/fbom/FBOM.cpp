#include "FBOM.hpp"
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <util/fs/FsUtil.hpp>

#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <asset/serialization/fbom/marshals/MeshMarshal.hpp>

namespace hyperion::v2::fbom {

FBOM::FBOM()
{
    // register loaders
    RegisterLoader<Entity>();
    RegisterLoader<Mesh>();
}

FBOM::~FBOM() = default;

} // namespace hyperion::v2::fbom