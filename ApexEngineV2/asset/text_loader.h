#ifndef TEXT_LOADER_H
#define TEXT_LOADER_H

#include "asset_loader.h"

namespace apex {
class TextLoader : public AssetLoader {
public:
    std::shared_ptr<Loadable> LoadFromFile(const std::string &);

    class LoadedText : public Loadable {
    public:
        LoadedText(const std::string &text)
            : text(text)
        {
        }

        const std::string &GetText() const
        {
            return text;
        }

        virtual std::shared_ptr<Loadable> Clone();

    private:
        std::string text;

        std::shared_ptr<LoadedText> CloneImpl();
    };
};
}

#endif
