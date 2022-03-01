#ifndef DECLARED_UNIFORM_H
#define DECLARED_UNIFORM_H

#include "uniform.h"

namespace hyperion {

struct DeclaredUniform {
    using Id_t = int;

    Id_t id;
    std::string name;
    Uniform value;

    DeclaredUniform(Id_t id, const std::string &name, Uniform value = Uniform())
        : id(id), name(name), value(value)
    {
    }

    DeclaredUniform(const DeclaredUniform &other)
        : id(other.id), name(other.name), value(other.value)
    {
    }

    inline DeclaredUniform &operator=(const DeclaredUniform &other)
    {
        id = other.id;
        name = other.name;
        value = other.value;

        return *this;
    }
};

struct UniformResult {
    enum {
        DECLARED_UNIFORM_OK,
        DECLARED_UNIFORM_ERR
    } result;

    DeclaredUniform::Id_t id;

    std::string message;

    UniformResult(decltype(result) result, DeclaredUniform::Id_t id = -1, const std::string &message = "")
        : result(result), message(message), id(id) {}
    UniformResult(const UniformResult &other)
        : result(other.result), message(other.message), id(other.id) {}
    inline UniformResult &operator=(const UniformResult &other)
    {
        result = other.result;
        id = other.id;
        message = other.message;

        return *this;
    }

    inline explicit operator bool() const { return result == DECLARED_UNIFORM_OK; }
};

} // namespace hyperion

#endif