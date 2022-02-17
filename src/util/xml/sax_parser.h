#ifndef SAXPARSER_H
#define SAXPARSER_H

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace hyperion {
namespace xml {

typedef std::map<std::string, std::string> AttributeMap;

enum SaxResult {
    SAX_OK = 0,
    SAX_ERR = 1
};

class SaxHandler {
public:
    SaxHandler() {}
    virtual ~SaxHandler() {}

    virtual void Begin(const std::string &name, const AttributeMap &attributes) = 0;
    virtual void End(const std::string &name) = 0;
    virtual void Characters(const std::string &value) = 0;
    virtual void Comment(const std::string &comment) = 0;
};

class SaxParser {
public:
    SaxParser(SaxHandler *handler);
    SaxResult Parse(const std::string &filepath);

private:
    std::ifstream file;
    SaxHandler *handler;
};

} // namespace xml
} // namespace hyperion

#endif
