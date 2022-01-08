#include "shader_preprocessor.h"
#include "string_util.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"

#include <iostream>

namespace apex {

std::string ShaderPreprocessor::ProcessShader(const std::string &code, 
    std::map<std::string, float> defines, 
    const std::string &path)
{
    std::istringstream ss(code);
    std::ostringstream os;
    std::streampos pos;
    std::string line;

    std::string local_path(path.substr(0, path.find_last_of("\\/")));

    if (!(StringUtil::Contains(local_path, "/") ||
        StringUtil::Contains(local_path, "\\"))) {
        local_path.clear();
    }

    return ProcessInner(ss, pos, defines, local_path);
}

std::string ShaderPreprocessor::ProcessInner(std::istringstream &ss, 
    std::streampos &pos,
    std::map<std::string, float> &defines,
    const std::string &local_path)
{
    std::string res;
    std::string line;
    int if_layer = 0;
    bool state = true;
    while (std::getline(ss, line)) {
        line = StringUtil::Trim(line);

        if (StringUtil::StartsWith(line, "#define $")) {
            if (state) {
                std::string val = line.substr(9);

                const std::vector<std::string> components = StringUtil::Split(val, ' ');
    
                std::vector<std::string> new_components;
                new_components.reserve(components.size());

                for (const auto &item : components) {
                    const auto trimmed = StringUtil::Trim(item);

                    if (!trimmed.empty()) {
                        new_components.push_back(trimmed);
                    }
                }

                if (new_components.size() == 2) {
                    // TODO: allow for defines of any type
                    defines[new_components[0]] = std::stof(new_components[1]);
                } else {
                    res += "#error \"The `#define $` directive must be defined in the format: `#define $NAME value`\"\n";
                }
            }
        } else if (StringUtil::StartsWith(line, "#if !")) {
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

                const std::string include_path = local_path + "/" + path;

                if (auto loaded = AssetManager::GetInstance()->
                    LoadFromFile<TextLoader::LoadedText>(include_path)) {

                    std::string relative_path(loaded->GetFilePath().substr(0,
                        loaded->GetFilePath().find_last_of("\\/")));

                    if (!(StringUtil::Contains(relative_path, "/") ||
                        StringUtil::Contains(relative_path, "\\"))) {
                        relative_path.clear();
                    }

                    res += ProcessShader(loaded->GetText(), defines, relative_path) + "\n";
                } else {
                    res += "#error \"The include could not be found at: ";
                    res += include_path;
                    res += "\"\n";
                }
            }
        } else {
            if (state) {
                res += line + "\n";
            }
        }
    }

    std::string str;
    for (size_t i = 0; i < res.length(); i++) {
        char ch = res[i];
        if (ch == '$') {
            size_t start_pos = i;
            str = "";
            ch = res[++i];

            while (std::isalnum(ch) || ch == '_') {
                str += ch;
                ch = res[++i];
            }

            size_t len = i - start_pos;
            auto it = defines.find(str);
            if (it != defines.end()) {
                std::string val;
                if (it->second == (int)it->second) {
                    val = std::to_string((int)it->second);
                } else {
                    val = std::to_string(it->second);
                }
                res = res.replace(start_pos, len, val);
            }
        }
    }

    return res;
}

} // namespace apex
