#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>
#include <cstdlib>
#include <cctype>

namespace hyperion {

class StringUtil
{
public:
    static inline std::string ToLower(const std::string &str)
    {
        std::string result(str);

        std::transform(result.begin(), result.end(), result.begin(), [](char ch) {
            return std::tolower(ch);
        });

        return result;
    }

    static inline std::string ToUpper(const std::string &str)
    {
        std::string result(str);

        std::transform(result.begin(), result.end(), result.begin(), [](char ch) {
            return std::toupper(ch);
        });

        return result;
    }

    static inline bool StartsWith(const std::string &text, const std::string &token)
    {
        if (text.length() < token.length()) {
            return false;
        }
        return (text.compare(0, token.length(), token) == 0);
    }

    static inline bool EndsWith(const std::string &text, const std::string &token)
    {
        if (text.length() < token.length()) {
            return false;
        }

        return std::equal(text.begin() + text.size() - token.size(),
            text.end(), token.begin());
    }

    static inline bool Contains(const std::string &text, const std::string &token)
    {
        return text.find(token) != std::string::npos;
    }

    template <class LambdaFunction>
    static inline void SplitBuffered(const std::string &text, char sep, LambdaFunction func)
    {
        std::string accum;
        accum.reserve(1024);

        for (char ch : text) {
            if (ch == sep) {
                func(accum);
                accum.clear();

                continue;
            }

            accum += ch;
        }

        if (accum.length()) {
            func(accum);
        }
    }

    static inline std::vector<std::string> Split(const std::string &text, char sep)
    {
        std::vector<std::string> tokens;
        std::string working_string;
        working_string.reserve(text.length());

        for (char ch : text) {
            if (ch == sep) {
                tokens.push_back(working_string);
                working_string.clear();
                continue;
            }

            working_string += ch;
        }

        if (!working_string.empty()) {
            tokens.push_back(working_string);
        }

        return tokens;
    }

    static inline std::vector<std::string> RemoveEmpty(const std::vector<std::string> &strings)
    {
        std::vector<std::string> res;
        res.reserve(strings.size());

        for (auto &&str : strings) {
            if (!str.empty()) {
                res.push_back(str);
            }
        }

        return res;
    }

    static inline std::string TrimLeft(const std::string &s)
    {
        std::string res(s);
        res.erase(res.begin(), std::find_if(res.begin(), res.end(),
            [](int c) { return !std::isspace(c); }));
        return res;
    }

    static inline std::string TrimRight(const std::string &s)
    {
        std::string res(s);
        res.erase(std::find_if(res.rbegin(), res.rend(),
            [](int c) { return !std::isspace(c); }).base(), res.end());
        return res;
    }

    static inline std::string Trim(const std::string &s)
    {
        return TrimLeft(TrimRight(s));
    }

    template <size_t Size>
    static inline std::string Join(const std::array<std::string, Size> &args, const std::string &join_by)
    {
        const size_t count = args.size();

        std::stringstream ss;
        size_t i = 0;

        for (auto &str : args) {
            ss << str;

            if (i != count - 1 && !EndsWith(str, join_by)) {
                ss << join_by;
            }

            i++;
        }

        return ss.str();
    }

    static inline std::string ReplaceAll(const std::string &text,
        const std::string &from, const std::string &to)
    {
        std::string result(text);
        if (from.empty()) {
            return result;
        }
        size_t start_pos = 0;
        while ((start_pos = text.find(from, start_pos)) != std::string::npos) {
            result.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return result;
    }

    static inline std::string Basename(const std::string &filepath)
    {
        if (!Contains(filepath, "/") && !Contains(filepath, "\\")) {
            return filepath;
        }

        return filepath.substr(filepath.find_last_of("\\/") + 1);
    }

    static inline std::string BasePath(const std::string &filepath)
    {
        if (!Contains(filepath, "/") && !Contains(filepath, "\\")) {
            return "";
        }

        return filepath.substr(0, filepath.find_last_of("\\/"));
    }

    static inline std::vector<std::string> SplitPath(const std::string &str)
    {
        std::vector<std::string> res;
        
        std::string tmp;
        for (char ch : str) {
            if (ch == '\\' || ch == '/') {
                if (!tmp.empty()) {
                    res.emplace_back(tmp);
                    tmp.clear();
                }
                continue;
            }

            tmp += ch;
        }

        // add last
        if (!tmp.empty()) {
            res.emplace_back(tmp);
        }

        return res;
    }

    static inline std::vector<std::string> CanonicalizePath(const std::vector<std::string> &original)
    {
        std::vector<std::string> res;

        for (const auto &str : original) {
            if (str == ".." && !res.empty()) {
                res.pop_back();
            } else if (str != ".") {
                res.emplace_back(str);
            }
        }

        return res;
    }

    static inline std::string PathToString(const std::vector<std::string> &path)
    {
        std::string res;
        
        for (size_t i = 0; i < path.size(); i++) {
            res += path[i];
            if (i != path.size() - 1) {
                res += "/";
            }
        }

        return res;
    }

    static inline std::string StripExtension(const std::string &filename)
    {
        auto pos = filename.find_last_of(".");

        if (pos == std::string::npos) {
            return filename;
        }

        return filename.substr(0, pos);
    }

    static inline std::string GetExtension(const std::string &path)
    {
        std::vector<std::string> split_path = SplitPath(path);
        const std::string &filename = split_path.back();

        auto pos = filename.find_last_of('.');

        if (pos == std::string::npos) {
            return "";
        }

        return path.substr(pos + 1);
    }
    
    static inline bool Parse(const std::string &str, int *out_value)
    {
        int c = 0, sign = 0, x = 0;
        const char *p = str.c_str();

        for(c = *(p++); (c < 48 || c > 57); c = *(p++)) {if (c == 45) {sign = 1; c = *(p++); break;}}; // eat whitespaces and check sign
        for(; c > 47 && c < 58; c = *(p++)) x = (x << 1) + (x << 3) + c - 48;
        
        *out_value = sign ? -x : x;

        return true;
    }
    
    static inline bool Parse(const std::string &str, long *out_value)
    {
        *out_value = std::strtol(str.c_str(), nullptr, 0);

        return true;
    }
    
    static inline bool Parse(const std::string &str, long long *out_value)
    {
        *out_value = std::strtoll(str.c_str(), nullptr, 0);

        return true;
    }
    
    static inline bool Parse(const std::string &str, unsigned int *out_value)
    {
        unsigned int val = 0;
        const char *p = str.c_str();

        while (*p) {
            val = (val << 1) + (val << 3) + *(p++) - 48;
        }

        *out_value = val;

        return true;
    }
    
    static inline bool Parse(const std::string &str, float *out_value)
    {
        *out_value = std::strtof(str.c_str(), nullptr);

        return true;
    }
    
    static inline bool Parse(const std::string &str, double *out_value)
    {
        *out_value = std::strtod(str.c_str(), nullptr);

        return true;
    }

    template <typename T>
    static inline bool Parse(const std::string &str, T *out_value)
    {
        std::istringstream ss(str);
        T value;

        if (!(ss >> std::boolalpha >> value)) {
            return false;
        }

        *out_value = value;

        return true;
    }

    template <typename T>
    static inline T Parse(const std::string &str)
    {
        T value;

        Parse(str, &value);

        return value;
    }

    template <typename T>
    static inline bool IsNumber(const std::string &str) {
        T value{};

        return Parse<T>(str, &value);
    }
};

} // namespace hyperion

#endif
