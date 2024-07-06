#include <rendering/RenderProxy.hpp>
#include <rendering/SafeDeleter.hpp>

#include <scene/Entity.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

extern HYP_API SafeDeleter *g_safe_deleter;

void RenderProxyList::Add(ID<Entity> entity, const RenderProxy &proxy)
{
    FlatMap<ID<Entity>, RenderProxy>::Iterator iter = m_proxies.End();

    if (HasProxyForEntity(entity)) {
        iter = m_proxies.Find(entity);
    }

    if (iter != m_proxies.End()) {
        if (proxy != iter->second) {
            // Sanity check
            AssertThrow(!m_changed_proxies.Contains(entity));

            // Mark as changed if it is found in the previous iteration
            m_changed_proxies.Insert({ entity, std::move(iter->second) });
            m_changed_entities.Set(entity.ToIndex(), true);

            iter->second = proxy;
        }
    } else {
        iter = m_proxies.Insert(entity, proxy).first;
    }

    m_next_entities.Set(entity.ToIndex(), true);
}

bool RenderProxyList::MarkToKeep(ID<Entity> entity)
{
    if (!m_previous_entities.Test(entity.ToIndex())) {
        return false;
    }

    m_next_entities.Set(entity.ToIndex(), true);

    return true;
}

void RenderProxyList::MarkToRemove(ID<Entity> entity)
{
    m_next_entities.Set(entity.ToIndex(), false);
}

void RenderProxyList::GetRemovedEntities(Array<ID<Entity>> &out_entities)
{
    HYP_SCOPE;

    Bitset removed_bits = GetRemovedEntities();

    out_entities.Reserve(removed_bits.Count());

    SizeType first_set_bit_index;

    while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != -1) {
        out_entities.PushBack(ID<Entity>::FromIndex(first_set_bit_index));

        removed_bits.Set(first_set_bit_index, false);
    }
}

void RenderProxyList::GetAddedEntities(Array<RenderProxy *> &out_entities, bool include_changed)
{
    HYP_SCOPE;
    
    Bitset newly_added_bits = GetAddedEntities();

    if (include_changed) {
        newly_added_bits |= GetChangedEntities();
    }

    out_entities.Reserve(newly_added_bits.Count());

    Bitset::BitIndex first_set_bit_index;

    while ((first_set_bit_index = newly_added_bits.FirstSetBitIndex()) != Bitset::not_found) {
        const ID<Entity> id = ID<Entity>::FromIndex(first_set_bit_index);

        auto it = m_proxies.Find(id);
        AssertThrow(it != m_proxies.End());

        out_entities.PushBack(&it->second);

        newly_added_bits.Set(first_set_bit_index, false);
    }
}

RenderProxy *RenderProxyList::GetProxyForEntity(ID<Entity> entity)
{
    const auto it = m_proxies.Find(entity);

    if (it == m_proxies.End()) {
        return nullptr;
    }

    return &it->second;
}

void RenderProxyList::Advance(RenderProxyListAdvanceAction action)
{
    HYP_SCOPE;

    { // Remove proxies for removed bits
        for (Bitset::BitIndex index : GetRemovedEntities()) {
            const ID<Entity> entity = ID<Entity>::FromIndex(index);

            const auto it = m_proxies.Find(entity);

            if (it != m_proxies.End()) {
                g_safe_deleter->SafeRelease(std::move(it->second));

                m_proxies.Erase(it);
            }
        }
    }

    switch (action) {
    case RenderProxyListAdvanceAction::CLEAR:
        // Next state starts out zeroed out -- and next call to Advance will remove proxies for these objs
        m_previous_entities = std::move(m_next_entities);

        break;
    case RenderProxyListAdvanceAction::PERSIST:
        m_previous_entities = m_next_entities;

        break;
    default:
        HYP_FAIL("Invalid RenderProxyListAdvanceAction");

        break;
    }

    if (m_changed_entities.Count() != 0) {
        // Release changed proxies (previous values)
        for (auto &it : m_changed_proxies) {
            g_safe_deleter->SafeRelease(std::move(it.second));
        }

        m_changed_entities.Clear();
        m_changed_proxies.Clear();
    }
}

} // namespace hyperion