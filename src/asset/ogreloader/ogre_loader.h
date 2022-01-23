#ifndef OGRE_LOADER_H
#define OGRE_LOADER_H

#include "../asset_loader.h"
#include "../../util/xml/sax_parser.h"

namespace hyperion {
class OgreLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);
};
} // namespace hyperion

#endif
