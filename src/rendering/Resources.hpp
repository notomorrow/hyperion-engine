#ifndef HYPERION_V2_RESOURCES_H
#define HYPERION_V2_RESOURCES_H

#include <core/Containers.hpp>
#include "Shader.hpp"
#include "Framebuffer.hpp"
#include "Compute.hpp"
#include "Renderer.hpp"
#include "Material.hpp"
#include "Texture.hpp"
#include "Mesh.hpp"
#include "Light.hpp"
#include <ui/UIScene.hpp>
#include "../animation/Skeleton.hpp"
#include "../scene/Scene.hpp"
#include <camera/Camera.hpp>
#include <rendering/rt/BLAS.hpp>
#include <rendering/rt/TLAS.hpp>

#include <mutex>
#include <thread>

namespace hyperion::v2 {

class Engine;

// template <class Resource>
// class RefCounterTypeMap {
// public:
//     using TypeID = UInt;
//     using Map    = FlatMap<TypeID, std::unique_ptr<RefCounter<Resource, Engine *>>>;

// private:
//     static inline std::atomic<TypeID> id_counter;

// public:
//     template <class T>
//     static TypeID GetTypeID()
//     {
//         static_assert(std::is_base_of_v<Resource, T>, "Object must be a derived class of the Resource class");

//         static const TypeID id = ++id_counter;

//         return id;
//     }

//     using Iterator      = typename Map::Iterator;
//     using ConstIterator = typename Map::ConstIterator;

//     RefCounterTypeMap() = default;
//     RefCounterTypeMap(const RefCounterTypeMap &other) = delete;
//     RefCounterTypeMap &operator=(const RefCounterTypeMap &other) = delete;

//     RefCounterTypeMap(RefCounterTypeMap &&other) noexcept
//         : m_map(std::move(other.m_map))
//     {
//     }

//     RefCounterTypeMap &operator=(RefCounterTypeMap &&other) noexcept
//     {
//         m_map = std::move(other.m_map);

//         return *this;
//     }

//     ~RefCounterTypeMap() = default;
    
//     template <class T>
//     void Set(RefCounter<T, Engine *> &&ref_counter)
//     {
//         AssertThrow(ref_counter != nullptr);

//         const auto id = GetTypeID<T>();

//         m_map[id] = std::move(component);
//     }

//     template <class T>
//     T *Get() const
//     {
//         const auto id = GetTypeID<T>();

//         const auto it = m_map.Find(id);

//         if (it == m_map.End()) {
//             return nullptr;
//         }

//         return static_cast<RefCounter<T>>(it->second);
//     }

//     Component *Get(ComponentId id) const
//     {
//         const auto it = m_map.Find(id);

//         if (it == m_map.End()) {
//             return nullptr;
//         }

//         return it->second.get();
//     }

//     template <class T>
//     bool Has() const
//     {
//         const auto id = GetComponentId<T>();

//         return m_map.Find(id) != m_map.End();
//     }
    
//     bool Has(ComponentId id) const
//     {
//         return m_map.Find(id) != m_map.End();
//     }
    
//     template <class T>
//     bool Remove()
//     {
//         const auto id = GetComponentId<T>();

//         const auto it = m_map.Find(id);

//         if (it == m_map.End()) {
//             return false;
//         }

//         m_map.Erase(it);

//         return true;
//     }
    
//     bool Remove(ComponentId id)
//     {
//         const auto it = m_map.Find(id);

//         if (it == m_map.End()) {
//             return false;
//         }

//         m_map.Erase(it);

//         return true;
//     }

//     void Clear()
//     {
//         m_map.Clear();
//     }

//     HYP_DEF_STL_BEGIN_END(m_map.begin(), m_map.end());

// private:
//     Map m_map;
// };

struct Resources {
#define HYP_DEF_REF_COUNTED(class_name, member_name) \
    RefCounter<class_name, Engine *> member_name

    HYP_DEF_REF_COUNTED(Shader,            shaders);
    HYP_DEF_REF_COUNTED(Texture,           textures);
    HYP_DEF_REF_COUNTED(Framebuffer,       framebuffers);
    HYP_DEF_REF_COUNTED(RenderPass,        render_passes);
    HYP_DEF_REF_COUNTED(Material,          materials);
    HYP_DEF_REF_COUNTED(Light,             lights);
    HYP_DEF_REF_COUNTED(RendererInstance,  renderer_instances);
    HYP_DEF_REF_COUNTED(ComputePipeline,   compute_pipelines);
    HYP_DEF_REF_COUNTED(Entity,            entities);
    HYP_DEF_REF_COUNTED(Mesh,              meshes);
    HYP_DEF_REF_COUNTED(Skeleton,          skeletons);
    HYP_DEF_REF_COUNTED(Scene,             scenes);
    HYP_DEF_REF_COUNTED(Blas,              blas);
    HYP_DEF_REF_COUNTED(Camera,            cameras);
    //HYP_DEF_REF_COUNTED(UIObject,          ui_objects);

#undef HYP_DEF_REF_COUNTED

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
    Engine *m_engine;
    std::mutex m_mtx;
};

} // namespace hyperion::v2

#endif