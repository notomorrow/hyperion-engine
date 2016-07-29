#include "shader_preprocessor.h"
#include "string_util.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"

#include <iostream>

namespace apex {
std::string ShaderPreprocessor::ProcessShader(const std::string &code, 
    const std::map<std::string, float> &defines, 
    const std::string &path)
{
    std::istringstream ss(code);
    std::ostringstream os;
    std::streampos pos;
    std::string line;

    std::string local_path(path.substr(0,
        path.find_last_of("\\/")));
    if (!(StringUtil::Contains(local_path, "/") ||
        StringUtil::Contains(local_path, "\\"))) {
        local_path.clear();
    }

    return ProcessInner(ss, pos, defines, local_path);
}

std::string ShaderPreprocessor::ProcessInner(std::istringstream &ss, 
    std::streampos &pos,
    const std::map<std::string, float> &defines,
    const std::string &local_path)
{
    std::string res;
    std::string line;
    int if_layer = 0;
    bool state = true;
    while (std::getline(ss, line)) {
        line = StringUtil::Trim(line);

        if (StringUtil::StartsWith(line, "#if !")) {
            if (state) {
                std::string val = line.substr(5);
                auto it = defines.find(val);
                if (it != defines.end() && it->second) {
                    ++if_layer;
                    state = false;
                } else {
                    res += ProcessInner(ss, pos, defines, local_path) + "\n";
                }
            }
        } else if (StringUtil::StartsWith(line, "#if ")) {
            if (state) {
                std::string val = line.substr(4);
                auto it = defines.find(val);
                if (it != defines.end() && it->second) {
                    res += ProcessInner(ss, pos, defines, local_path) + "\n";
                } else {
                    ++if_layer;
                    state = false;
                }
            }
        } else if (StringUtil::StartsWith(line, "#endif")) {
            --if_layer;
            if (if_layer == 0) {
                // exit #if branches, return to normal state
                state = true;
            } else if (if_layer == -1) {
                // we aren't in first recursion, #if part already read
                return res; // exit current recursion
            }
        } else if (StringUtil::StartsWith(line, "#include ")) {
            if (state) {
                std::string val = line.substr(9);
                std::string path;
                if (val[0] == '\"') {
                    int idx = 1;
                    while (idx < val.size() && val[idx] != '\"') {
                        path += val[idx];
                        ++idx;
                    }
                }

                auto loaded = AssetManager::GetInstance()->
                    LoadFromFile<TextLoader::LoadedText>(local_path + "/" + path);

                std::string relative_path(loaded->GetFilePath().substr(0,
                    loaded->GetFilePath().find_last_of("\\/")));
                if (!(StringUtil::Contains(relative_path, "/") ||
                    StringUtil::Contains(relative_path, "\\"))) {
                    relative_path.clear();
                }

                res += ProcessShader(loaded->GetText(), defines, relative_path) + "\n";
            }
        } else {
            if (state) {
                res += line + "\n";
            }
        }
    }
    
    // replace %define with value
    for (auto &&def : defines) {
        std::string str;
        // check if it has decimal
        if (def.second == (int)def.second) {
            str = std::to_string((int)def.second);
        } else {
            str = std::to_string(def.second);
        }
        res = StringUtil::ReplaceAll(res, "%" + def.first, str);
    }

    return res;
}
}