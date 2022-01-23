#ifndef WAV_LOADER_H
#define WAV_LOADER_H

#include "../asset/asset_loader.h"

namespace hyperion {
class WavLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);
};
} // namespace hyperion

#endif
