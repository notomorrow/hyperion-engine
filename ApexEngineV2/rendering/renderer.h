#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <map>
#include <cstdlib>

#include "../core_engine.h"
#include "../entity.h"
#include "../hash_code.h"
#include "math/bounding_box.h"
#include "render_window.h"
#include "material.h"
#include "camera/camera.h"
#include "framebuffer.h"
#include "postprocess/post_processing.h"

namespace apex {
/*class MemoizedBoundingBox : public BoundingBox {
    MemoizedBoundingBox()
        : BoundingBox()
    {   
    }

    MemoizedBoundingBox(const Vector3 &min, const Vector3 &max)
        : BoundingBox(min, max)
    {
    }

    MemoizedBoundingBox(const BoundingBox &other)
        : BoundingBox(other)
    {
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(GetMin().GetHashCode());
        hc.Add(Max().GetHashCode());

        return hc;
    }
};

class MemoizedFrustum : public Frustum {
    MemoizedFrustum()
        : Frustum()
    {   
    }

    MemoizedFrustum(const Frustum &other)
        : Frustum(other)
    {
    }

    MemoizedFrustum(const Matrix4 &view_proj)
        : Frustum(view_proj)
    {
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (const Vector4 &plane : m_planes) {
            hc.Add(plane.GetHashCode());
        }

        return hc;
    }
};*/

struct MemoizedFrustumCheckKey {
    size_t frustum_hash_code;
    size_t aabb_hash_code;
};


struct BucketItem {
    Renderable *renderable;
    Material *material;
    BoundingBox aabb;
    Transform transform;
    size_t hash_code;
    bool frustum_culled;
    bool alive;

    BucketItem()
        : renderable(nullptr),
          material(nullptr),
          aabb(),
          transform(),
          hash_code(0),
          frustum_culled(false),
          alive(true)
    {
    }

    BucketItem(const BucketItem &other)
        : renderable(other.renderable),
          material(other.material),
          aabb(other.aabb),
          transform(other.transform),
          hash_code(other.hash_code),
          frustum_culled(other.frustum_culled),
          alive(other.alive)
    {
    }
};

struct Bucket {
    bool enable_culling;

    Bucket()
        : enable_culling(true)
    {
    }

    Bucket(const Bucket &other)
        : enable_culling(other.enable_culling),
          items(other.items),
          hash_to_item_index(other.hash_to_item_index)
    {
    }

    inline const std::vector<BucketItem> &GetItems() const
    {
        return items;
    }

    std::size_t GetIndex(size_t at)
    {
        const auto it = hash_to_item_index.find(at);
        assert(it != hash_to_item_index.end());

        return it->second;
    }

    BucketItem *GetItemPtr(size_t at)
    {
        const auto it = hash_to_item_index.find(at);

        if (it == hash_to_item_index.end())
        {
            return nullptr;
        }

        const size_t index = it->second;
        assert(index < items.size());

        return &items[index];
    }

    BucketItem &GetItem(size_t at)
    {
        const auto it = hash_to_item_index.find(at);
        assert(it != hash_to_item_index.end());

        const size_t index = it->second;
        assert(index < items.size());

        return items[index];
    }

    void AddItem(const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(bucket_item.hash_code);
        assert(it == hash_to_item_index.end());

        const size_t index = items.size();

        items.push_back(bucket_item);
        hash_to_item_index[bucket_item.hash_code] = index;
    }

    void SetItem(size_t at, const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(at);
        assert(it != hash_to_item_index.end());

        const size_t index = it->second;
        assert(index < items.size());

        items[index] = bucket_item;

        if (bucket_item.hash_code != at) {
            hash_to_item_index.erase(it);
            hash_to_item_index[bucket_item.hash_code] = index;
        }
    }

    void RemoveItem(size_t at)
    {
        const auto it = hash_to_item_index.find(at);
        assert(it != hash_to_item_index.end());

        const size_t index = it->second;
        assert(index < items.size());

        hash_to_item_index.erase(it);
        //items.erase(items.begin() + index);

        // refrain from erasing in the middle of the vector,
        // which would lose indexes

        items[index].alive = false;

        if (index == items.size() - 1) {
            items.pop_back();
        }
    }

    void ClearAll()
    {
        items.clear();
    }

private:

    std::vector<BucketItem> items;
    std::map<std::size_t, std::size_t> hash_to_item_index;
};

using Bucket_t = std::vector<BucketItem>;

class Shader;

class Renderer {
public:
    Renderer(const RenderWindow &);
    ~Renderer();

    void Begin(Camera *cam, Entity *top);
    void Render(Camera *cam);
    void End(Camera * cam, Entity *top);

    void RenderBucket(Camera *cam, Bucket &bucket, Shader *override_shader = nullptr);
    void RenderAll(Camera *cam, Framebuffer *fbo = nullptr);
    void RenderPost(Camera *cam, Framebuffer *fbo);
 
    inline PostProcessing *GetPostProcessing() { return m_post_processing; }
    inline const PostProcessing *GetPostProcessing() const { return m_post_processing; }
    inline RenderWindow &GetRenderWindow() { return m_render_window; }
    inline const RenderWindow &GetRenderWindow() const { return m_render_window; }

    inline Bucket &GetBucket(Renderable::RenderBucket bucket) { return m_buckets[bucket]; }

    Bucket m_buckets[6];

private:
    PostProcessing *m_post_processing;
    Framebuffer *m_fbo;
    RenderWindow m_render_window;

    std::map<Entity*, std::size_t> m_hash_cache;
    std::map<std::size_t, Renderable::RenderBucket> m_hash_to_bucket;

    void ClearRenderables();
    void FindRenderables(Camera *cam, Entity *top, bool frustum_culled = false, bool is_root = false);
    bool MemoizedFrustumCheck(Camera *cam, const BoundingBox &aabb);

    std::map<MemoizedFrustumCheckKey, bool> m_memoized_frustum_checks; // TODO: some sort of deque map
};
} // namespace apex

#endif
