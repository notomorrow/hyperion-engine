#ifndef HYPERION_V2_SCRIPT_LOADER_H
#define HYPERION_V2_SCRIPT_LOADER_H

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>
#include <script/Script.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class ScriptLoader : public AssetLoader
{
public:
    virtual ~ScriptLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;

    SourceFile source_file;
};

} // namespace hyperion::v2

#endif
