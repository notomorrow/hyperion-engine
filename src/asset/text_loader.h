#ifndef TEXT_LOADER_H
#define TEXT_LOADER_H

#include "asset_loader.h"

namespace hyperion {
class TextLoader : public AssetLoader {
public:
    class LoadedText;

    std::shared_ptr<Loadable> LoadFromFile(const std::string &);

    // convenience function, as many other loaders may use TextLoader
    inline std::shared_ptr<LoadedText> LoadTextFromFile(const std::string &str)
    {
        if (auto text = LoadFromFile(str)) {
            return std::static_pointer_cast<LoadedText>(text);
        }

        return nullptr;
    }

    class LoadedText : public Loadable {
    public:
        LoadedText(const std::string &text)
            : text(text)
        {
        }

        inline const std::string &GetText() const {  return text; }

        virtual std::shared_ptr<Loadable> Clone();

    private:
        std::string text;

        std::shared_ptr<LoadedText> CloneImpl();
    };
};
}

#endif
