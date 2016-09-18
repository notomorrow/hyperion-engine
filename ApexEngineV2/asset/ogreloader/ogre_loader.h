#ifndef OGRE_LOADER_H
#define OGRE_LOADER_H

#include "../asset_loader.h"
#include "../../util/xml/sax_parser.h"

namespace apex {
class OgreLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);
};
} // namespace apex

#endif