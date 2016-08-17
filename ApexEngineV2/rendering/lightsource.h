#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H

namespace apex {
class Shader;
class LightSource {
public:
    virtual ~LightSource() = default;
    virtual void BindLight(int index, Shader *shader) = 0;
};
}

#endif