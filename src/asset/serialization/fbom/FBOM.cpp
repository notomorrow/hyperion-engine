#include "FBOM.hpp"
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <util/fs/FsUtil.hpp>

#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <asset/serialization/fbom/marshals/MeshMarshal.hpp>
#include <asset/serialization/fbom/marshals/ShaderMarshal.hpp>
#include <asset/serialization/fbom/marshals/SubShaderMarshal.hpp>
#include <asset/serialization/fbom/marshals/MaterialMarshal.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>

namespace hyperion::v2::fbom {

FBOM::FBOM()
{
    // register loaders
    RegisterLoader<Entity>();
    RegisterLoader<Mesh>();
    RegisterLoader<Shader>();
    RegisterLoader<SubShader>();
    RegisterLoader<Material>();
    RegisterLoader<Node>();
}

FBOM::~FBOM() = default;

} // namespace hyperion::v2::fbom