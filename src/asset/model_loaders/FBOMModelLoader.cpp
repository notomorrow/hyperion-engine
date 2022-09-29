#include "FBOMModelLoader.hpp"
#include <Engine.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <util/fs/FsUtil.hpp>

#include <algorithm>
#include <stack>
#include <string>


namespace hyperion::v2 {

LoadedAsset FBOMModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);
    auto *engine = state.asset_manager->GetEngine();
    AssertThrow(engine != nullptr);

    fbom::FBOMReader reader(engine, fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject object;

    if (auto err = reader.LoadFromFile(String(state.filepath.c_str()), object)) {
        return { { LoaderResult::Status::ERR, err.message } };
    }

    return { { LoaderResult::Status::OK }, std::move(object.m_value) };
}

} // namespace hyperion::v2
