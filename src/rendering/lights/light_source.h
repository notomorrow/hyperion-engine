#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

namespace hyperion {

class Shader;

class LightSource {
public:
    virtual ~LightSource() = default;
    virtual void Bind(int index, Shader *shader) const = 0;
};

} // namespace hyperion

#endif
