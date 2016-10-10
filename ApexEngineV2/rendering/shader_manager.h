#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include "shader.h"

#include <map>
#include <vector>
#include <utility>
#include <string>

namespace apex {

class ShaderManager {
public:
    static ShaderManager *instance;
    static ShaderManager *GetInstance();

    template <typename T>
    std::shared_ptr<T> GetShader(const ShaderProperties &properties)
    {
        static_assert(std::is_base_of<Shader, T>::value,
            "T must be a derived class of Shader");

        static_assert(std::is_constructible<T, const ShaderProperties &>::value,
            "T must be constructable with: const ShaderProperties &");

        for (auto &&ins : instances) {
            auto ins_casted = std::dynamic_pointer_cast<T>(ins.first);
            if (ins_casted && properties == ins.second) {
                return ins_casted;
            }
        }

        auto new_ins = std::make_shared<T>(properties);
        instances.push_back(std::make_pair(new_ins, properties));

        std::cout << "New shader instance, type: " << typeid(T).name() << "\n";
        return new_ins;
    }

private:
    std::vector<
        std::pair<
            std::shared_ptr<Shader>, 
            ShaderProperties
        >
    > instances;
};

} // namespace apex

#endif