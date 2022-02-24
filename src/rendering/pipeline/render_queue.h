#ifndef RENDER_QUEUE_H
#define RENDER_QUEUE_H

#define RENDERER_SHADER_GROUPING 1
#define RENDERER_FRUSTUM_CULLING 1

namespace hyperion {

struct BucketItem {
    int m_id;
    Spatial m_spatial;
    bool alive;

    BucketItem(int id)
        : m_id(id),
          alive(true)
    {
    }

    BucketItem(int id, const Spatial &spatial)
        : m_id(id),
          m_spatial(spatial),
          alive(true)
    {
    }

    BucketItem(const BucketItem &other)
        : m_id(other.m_id),
          m_spatial(other.m_spatial),
          alive(other.alive)
    {
    }

    inline const std::shared_ptr<Renderable> &GetRenderable() const { return m_spatial.GetRenderable(); }
    inline const Spatial &GetSpatial() const { return m_spatial; }
    inline int GetId() const { return m_id; }
};

struct Bucket {
    bool enable_culling;

    Bucket()
        : enable_culling(true)
    {
    }

    /*Bucket(const Bucket &other)
        : enable_culling(other.enable_culling),
          items(other.items),
          hash_to_item_index(other.hash_to_item_index)
    {
    }*/

    Bucket(const Bucket &other) = delete;
    inline Bucket &operator=(const Bucket &other) = delete;

    inline bool IsEmpty() const { return items.empty(); }
    inline const std::vector<BucketItem> &GetItems() const { return items; }

    std::size_t GetIndex(int id)
    {
        const auto it = hash_to_item_index.find(id);
        ex_assert(it != hash_to_item_index.end());

        return it->second;
    }

    BucketItem *GetItemPtr(int id)
    {
        const auto it = hash_to_item_index.find(id);

        if (it == hash_to_item_index.end())
        {
            return nullptr;
        }

        const size_t index = it->second;
        ex_assert(index < items.size());

        return &items[index];
    }

    BucketItem &GetItem(int id)
    {
        const auto it = hash_to_item_index.find(id);
        ex_assert(it != hash_to_item_index.end());

        const size_t index = it->second;
        ex_assert(index < items.size());

        return items[index];
    }

    void AddItem(const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(bucket_item.GetId());
        soft_assert(it == hash_to_item_index.end());

        soft_assert(bucket_item.GetRenderable() != nullptr);

        // For optimization: try to find an existing slot that
        // where the same shader is used previously or next
        // this will save us from having to switch shaders
        // as much.
        // if no slots are found where the shaders match,
        // use an existing (non alive) slot anyway,
        // that way we don't have to increase the array size.
        // it comes free anyway

        size_t slot_index = 0;
        bool slot_found = false;

#if RENDERER_SHADER_GROUPING
        for (size_t i = 0; i < items.size(); i++) {
            if (!items[i].alive) {
                slot_index = i;
                slot_found = true;

                soft_assert_break_msg(bucket_item.GetRenderable()->GetShader() != nullptr, "No shader set, using first found inactive slot");

                continue;
            }

            // it is alive -- check if "next" slot is empty
            bool is_next_empty = (i < items.size() - 1) && !items[i + 1].alive;
            bool is_prev_empty = (i > 0) && !items[i - 1].alive;

            if (!is_next_empty && !is_prev_empty) {
                continue;
            }

            // next one is empty, so we compare shaders for this,
            // checking if we should insert the item into the next slot

            hard_assert(items[i].GetRenderable() != nullptr);

            if (items[i].GetRenderable()->GetShader() != nullptr) {
                // doing ID check for now... not sure how portable it will be
                if (items[i].GetRenderable()->GetShader()->GetId() == bucket_item.GetRenderable()->GetShader()->GetId()) {
                    slot_index = is_next_empty ? (i + 1) : (i - 1);
                    slot_found = true;

                    std::cout << "Saved 1 switch of shaders\n";

                    break;
                }
            }
        }

        if (slot_found) {
            items[slot_index] = bucket_item;
        } else {
#endif
            slot_index = items.size();
            items.push_back(bucket_item);
#if RENDERER_SHADER_GROUPING
        }
#endif

        hash_to_item_index[bucket_item.GetId()] = slot_index;
    }

    void InsertOrUpdateItem(int id, const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(id);

        if (it == hash_to_item_index.end()) {
            AddItem(bucket_item);

            return;
        }

        const size_t index = it->second;
        hard_assert(index < items.size());

        items[index] = bucket_item;

        if (bucket_item.GetId() != id) {
            hash_to_item_index.erase(it);
            hash_to_item_index[id] = index;
        }
    }

    void UpdateItem(int id, const BucketItem &bucket_item)
    {
        const auto it = hash_to_item_index.find(id);
        soft_assert(it != hash_to_item_index.end());

        const size_t index = it->second;
        hard_assert(index < items.size());

        items[index] = bucket_item;

        if (bucket_item.GetId() != id) {
            hash_to_item_index.erase(it);
            hash_to_item_index[id] = index;
        }
    }

    void RemoveItem(int id)
    {
        const auto it = hash_to_item_index.find(id);
        soft_assert(it != hash_to_item_index.end());

        const size_t index = it->second;
        ex_assert(index < items.size());

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
        hash_to_item_index.clear();
    }

private:

    std::vector<BucketItem> items;
    std::map<HashCode::Value_t, std::size_t> hash_to_item_index;
};

using Bucket_t = std::vector<BucketItem>;

struct RenderQueue {
    Bucket m_buckets[Spatial::Bucket::RB_MAX];

    RenderQueue()
    {
        m_buckets[Spatial::Bucket::RB_SKY].enable_culling = false;
        m_buckets[Spatial::Bucket::RB_PARTICLE].enable_culling = false; // TODO
        m_buckets[Spatial::Bucket::RB_SCREEN].enable_culling = false;
        m_buckets[Spatial::Bucket::RB_DEBUG].enable_culling = false;
    }

    RenderQueue(const RenderQueue &other) = delete;
    RenderQueue &operator=(const RenderQueue &other) = delete;
    ~RenderQueue() = default;

    inline Bucket &GetBucket(Spatial::Bucket bucket) { return m_buckets[bucket]; }

    void Clear()
    {
        for (int i = 0; i < Spatial::Bucket::RB_MAX; i++) {
            m_buckets[i].ClearAll();
        }
    }
};

} // namespace hyperion

#endif