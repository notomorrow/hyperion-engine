#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H

namespace hyperion {

class Shader;

class LightSource {
public:
    virtual ~LightSource() = default;
    virtual void Bind(int index, Shader *shader) = 0;
};

} // namespace hyperion

#endif
