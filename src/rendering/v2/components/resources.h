#ifndef HYPERION_V2_RESOURCES_H
#define HYPERION_V2_RESOURCES_H

#include "shader.h"
#include "framebuffer.h"
#include "compute.h"
#include "graphics.h"
#include "material.h"
#include "texture.h"
#include "mesh.h"
#include "util.h"

#include <mutex>

namespace hyperion::v2 {

class Engine;

struct Resources {
    ObjectHolder<Shader>            shaders;
    ObjectHolder<Texture>           textures;
    ObjectHolder<Framebuffer>       framebuffers;
    ObjectHolder<RenderPass>        render_passes;
    ObjectHolder<Material>          materials;
    ObjectHolder<ComputePipeline>   compute_pipelines{.defer_create = true};

    //RefCountedObjectHolder<Spatial> spatials;
    ObjectHolder<Spatial>           spatials;
    RefCountedObjectHolder<Mesh>    meshes;

    Resources();
    Resources(const Resources &other) = delete;
    Resources &operator=(const Resources &other) = delete;
    ~Resources();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    template <class LambdaFunction>
    void Lock(LambdaFunction lambda)
    {
        DebugLog(
            LogType::Debug,
            "Locking resource mutex in thread %llu\n",
            std::hash<std::thread::id>{}(std::this_thread::get_id())
        );

        m_mtx.lock();

        DebugLog(
            LogType::Debug,
            "Locked resource mutex in thread %llu\n",
            std::hash<std::thread::id>{}(std::this_thread::get_id())
        );


        lambda(*this);

        DebugLog(
            LogType::Debug,
            "Unlocking resource mutex in thread %llu\n",
            std::hash<std::thread::id>{}(std::this_thread::get_id())
        );

        m_mtx.unlock();

        DebugLog(
            LogType::Debug,
            "Unlocked resource mutex in thread %llu\n",
            std::hash<std::thread::id>{}(std::this_thread::get_id())
        );

    }

private:
    std::mutex m_mtx;
};

} // namespace hyperion::v2

#endif