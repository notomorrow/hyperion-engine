#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include "loadable.h"

#include <string>
#include <memory>

namespace apex {
class AssetLoader {
public:
    virtual std::shared_ptr<Loadable> LoadFromFile(const std::string &) = 0;

    template <typename T>
    std::shared_ptr<T> LoadFromFile(const std::string &path)
    {
        return std::dynamic_pointer_cast<T>(LoadFromFile(path));
    }
};
}

#endif