/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorDelegates.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypMember.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

EditorDelegates &EditorDelegates::GetInstance()
{
    static EditorDelegates instance;

    return instance;
}

void EditorDelegates::AddNodeWatcher(Name watcher_key, const FlatSet<const HypProperty *> &properties_to_watch, Proc<void, Node *, const HypProperty *> &&proc)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    auto it = m_node_watchers.Find(watcher_key);

    if (it == m_node_watchers.End()) {
        it = m_node_watchers.Insert(watcher_key, NodeWatcher { }).first;
    }

    for (const HypProperty *property : properties_to_watch) {
        AssertThrow(property != nullptr);
    }

    NodeWatcher &node_watcher = it->second;
    node_watcher.properties_to_watch.Merge(properties_to_watch);
    node_watcher.OnChange.Bind(std::move(proc)).Detach();
}

void EditorDelegates::RemoveNodeWatcher(Name watcher_key)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    m_node_watchers.Erase(watcher_key);
}

void EditorDelegates::OnNodeUpdate(Node *node, const HypProperty *property)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(property != nullptr);

    for (Pair<Name, NodeWatcher> &it : m_node_watchers) {
        NodeWatcher &node_watcher = it.second;

        if (!node_watcher.properties_to_watch.Empty() && !node_watcher.properties_to_watch.Contains(property)) {
            continue;
        }

        node_watcher.OnChange(node, property);
    }
}

} // namespace hyperion

