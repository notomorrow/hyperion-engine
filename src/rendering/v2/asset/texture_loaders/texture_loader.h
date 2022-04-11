#ifndef HYPERION_V2_TEXTURE_LOADER_H
#define HYPERION_V2_TEXTURE_LOADER_H

#include <rendering/v2/asset/loader_object.h>
#include <rendering/v2/asset/loader.h>
#include <rendering/v2/components/texture.h>

namespace hyperion::v2 {

template <>
struct LoaderObject<Texture2D, LoaderFormat::TEXTURE_2D> {
    class Loader : public LoaderBase<Texture2D, LoaderFormat::TEXTURE_2D> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<Texture2D> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };

    std::vector<unsigned char> data;
    int width;
    int height;
    int num_components;
    Image::InternalFormat format;
};

} // namespace hyperion::v2

#endif