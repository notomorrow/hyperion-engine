#ifndef HYPERION_V2_RESOURCES_H
#define HYPERION_V2_RESOURCES_H

#include <core/containers.h>
#include "shader.h"
#include "framebuffer.h"
#include "compute.h"
#include "graphics.h"
#include "material.h"
#include "texture.h"
#include "mesh.h"
#include "light.h"
#include "../animation/skeleton.h"
#include "../scene/scene.h"
#include <rendering/rt/blas.h>
#include <rendering/rt/tlas.h>

#include <mutex>
#include <thread>

namespace hyperion::v2 {

class Engine;

struct Resources {
    using Callbacks = EngineCallbacks;

    RefCounter<Shader,      Callbacks>      shaders;
    RefCounter<Texture,     Callbacks>      textures;
    RefCounter<Framebuffer, Callbacks>      framebuffers;
    RefCounter<RenderPass,  Callbacks>      render_passes;
    RefCounter<Material,    Callbacks>      materials;
    RefCounter<Light,       Callbacks>      lights;

    RefCounter<GraphicsPipeline, Callbacks> graphics_pipelines;
    RefCounter<ComputePipeline, Callbacks>  compute_pipelines;
    
    RefCounter<Spatial,     Callbacks>      spatials;
    RefCounter<Mesh,        Callbacks>      meshes;
    RefCounter<Skeleton,    Callbacks>      skeletons;

    RefCounter<Scene,       Callbacks>      scenes;

    RefCounter<Blas,        Callbacks>      blas;

    Resources(Engine *);
    Resources(const Resources &other) = delete;
    Resources &operator=(const Resources &other) = delete;
    ~Resources();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    /*! \brief Guard the given lambda with a lock/unlock of the resources'
     *  internal mutex for the purposes of asset loading.
     *  @param lambda The lambda function to execute within the guard
     */
    template <class LambdaFunction>
    void Lock(LambdaFunction lambda)
    {
        std::lock_guard guard(m_mtx);

        lambda(*this);
    }

private:
    std::mutex m_mtx;
};

} // namespace hyperion::v2

#endif