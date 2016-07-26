#include "text_loader.h"
#include <fstream>
#include <sstream>

namespace apex {
std::shared_ptr<Loadable> TextLoader::LoadFromFile(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return std::make_shared<LoadedText>(buffer.str());
}
}