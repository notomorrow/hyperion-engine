#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include "asset_loader.h"

namespace apex {
class TextureLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);
};
}

#endif