#ifndef MTL_LOADER_H
#define MTL_LOADER_H

#include "../asset_loader.h"
#include "../../rendering/material.h"

#include <vector>
#include <utility>
#include <string>

namespace apex {

class MtlLib : public Loadable {
public:
    virtual ~MtlLib() = default;

    inline void NewMaterial(const std::string &name) {  m_materials.push_back({ name, Material() }); }
    inline Material *GetLastMaterial()
    {
        if (m_materials.empty()) {
            return nullptr;
        }
        return &m_materials.back().second;
    }

    virtual std::shared_ptr<Loadable> Clone() override;

private:
    std::vector<std::pair<std::string, Material>> m_materials;
};

class MtlLoader : public AssetLoader {
public:
    virtual ~MtlLoader() = default;

    std::shared_ptr<Loadable> LoadFromFile(const std::string &) override;

private:
    std::shared_ptr<Texture> LoadTexture(const std::string &name, const std::string &path);
};
}

#endif