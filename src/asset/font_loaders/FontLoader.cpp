#include <asset/font_loaders/FontLoader.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

LoadedAsset FontLoader::LoadAsset(LoaderState &state) const
{
    FontEngine &font_engine = FontEngine::GetInstance();

    auto font_face = UniquePtr<RC<Face>>::Construct(
        RC<Face>::Construct(font_engine.GetFontBackend(), state.filepath)
    );

    return { { LoaderResult::Status::OK }, font_face.Cast<void>() };
}

} // namespace hyperion::v2
