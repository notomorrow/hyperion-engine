#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H

namespace apex {

class Shader;

class LightSource {
public:
    virtual ~LightSource() = default;
    virtual void Bind(int index, Shader *shader) = 0;
};

} // namespace apex

#endif