#ifndef LOADABLE_H
#define LOADABLE_H

#include <string>
#include <memory>

namespace apex {
class Loadable {
public:
    virtual ~Loadable() = default;

    inline const std::string &GetFilePath() const { return m_filepath; }
    inline void SetFilePath(const std::string &filepath) { m_filepath = filepath; }

    virtual std::shared_ptr<Loadable> Clone() { return nullptr; };// = 0;

private:
    std::string m_filepath;
};
}

#endif