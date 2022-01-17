#include "shader_preprocessor.h"
#include "../rendering/shader.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "string_util.h"

#include <iostream>

namespace apex {

std::string ShaderPreprocessor::ProcessShader(const std::string &code, 
    const ShaderProperties &shader_properties, 
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

    return ProcessInner(ss, pos, shader_properties, local_path);
}

std::string ShaderPreprocessor::ProcessInner(std::istringstream &ss, 
    std::streampos &pos,
    const ShaderProperties &shader_properties,
    const std::string &local_path)
{
    ShaderProperties defines(shader_properties);

    std::string res;
    std::string line;
    int if_layer = 0;
    bool state = true;
    while (std::getline(ss, line)) {
        line = StringUtil::Trim(line);

        if (StringUtil::StartsWith(line, "#define $")) {
            if (state) {
                std::string sub = line.substr(9);

                const std::vector<std::string> components = StringUtil::Split(sub, ' ');
    
                std::vector<std::string> new_components;
                new_components.reserve(components.size());

                for (const auto &item : components) {
                    const auto trimmed = StringUtil::Trim(item);

                    if (!trimmed.empty()) {
                        new_components.push_back(trimmed);
                    }
                }

                if (new_components.size() >= 2) {
                    std::string key = new_components[0];
                    std::string value = line.substr(9 + key.size());

                    ShaderProperties::ShaderProperty::Value result;

                    if (new_components.size() == 2) {
                        if (StringUtil::ParseNumber<float>(value, &result.float_value)) {
                            defines.Define(key, result.float_value);
                        } else if (StringUtil::ParseNumber<int>(value, &result.int_value)) {
                            defines.Define(key, result.int_value);
                        } else if (StringUtil::ParseNumber<bool>(value, &result.bool_value)) {
                            defines.Define(key, result.bool_value);
                        } else {
                            defines.Define(key, value);
                        }
                    } else {
                        defines.Define(key, value);
                    }
                } else {
                    res += "#error \"The `#define $` directive must be defined in the format: `#define $NAME value`\"\n";
                }
            }
        } else if (StringUtil::StartsWith(line, "#if !")) {
            if (state) {
                std::string key = line.substr(5);

                if (defines.GetValue(key).IsTruthy()) {
                    ++if_layer;
                    state = false;
                } else {
                    res += ProcessInner(ss, pos, defines, local_path) + "\n";
                }
            }
        } else if (StringUtil::StartsWith(line, "#if ")) {
            if (state) {
                std::string key = line.substr(4);

                if (defines.GetValue(key).IsTruthy()) {
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

            auto value = defines.GetValue(str);

            res = res.replace(start_pos, len, value.raw_value);
        }
    }

    return res;
}

} // namespace apex
