#ifndef LOADABLE_H
#define LOADABLE_H

#include <string>

namespace apex {
class Loadable {
public:
    virtual ~Loadable()
    {
    }

    const std::string &GetFilePath() const
    {
        return filepath;
    }

    void SetFilePath(const std::string &path)
    {
        filepath = path;
    }

private:
    std::string filepath;
};
}

#endif