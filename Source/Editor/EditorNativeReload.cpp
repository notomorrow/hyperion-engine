/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorNativeReload.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/Csg/EditorCsgState.hpp>
#include <Editor/Prefab/EditorPrefabEditState.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentContainer.hpp>
#include <Scene/ComponentInterface.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Property.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

struct RebuiltNode
{
    Handle<Node> oldNode;
    Handle<Node> newNode;
};

static bool DerivesFromRetiredClass(const Class* cls, const Array<const Class*>& retiredClasses)
{
    for (; cls != nullptr; cls = cls->GetParent())
    {
        if (retiredClasses.Contains(cls))
        {
            return true;
        }
    }

    return false;
}

static const Handle<Node>* FindRebuilt(const ObjectBase* object, const Array<RebuiltNode>& rebuiltNodes)
{
    if (object == nullptr)
    {
        return nullptr;
    }

    for (const RebuiltNode& rebuiltNode : rebuiltNodes)
    {
        if (rebuiltNode.oldNode.Get() == object)
        {
            return &rebuiltNode.newNode;
        }
    }

    return nullptr;
}

static Array<Scene*> GetProjectScenes(EditorProject* project)
{
    Array<Scene*> scenes;

    if (const Handle<World>& world = project->GetWorld(); world.IsValid())
    {
        for (const Handle<Scene>& scene : world->GetScenes())
        {
            if (scene.IsValid() && scene->GetRoot().IsValid())
            {
                scenes.PushBack(scene.Get());
            }
        }
    }

    return scenes;
}

static Handle<Node> RebuildNode(const Handle<Node>& oldNode, const Class* newClass)
{
    Node* parent = oldNode->GetParent();

    if (parent == nullptr)
    {
        HYP_LOG(Editor, Warning, "{} has no parent, so it stays on the previous game module build", oldNode->GetName());

        return Handle<Node>::Null();
    }

    Handle<Node> newNode = oldNode->CloneAs(newClass, /* cloneChildren */ false);

    if (!newNode.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to rebuild {} as {}; it stays on the previous game module build", oldNode->GetName(), newClass->GetName());

        return Handle<Node>::Null();
    }

    newNode->SetUUID(oldNode->GetUUID());

    // added before the old node goes, so anything keyed by UUID ends up on the new one
    parent->InsertChild(newNode, oldNode->FindSelfIndex());

    Array<Handle<Node>> children;

    for (Node* child : oldNode->GetChildren())
    {
        children.PushBack(MakeStrongRef(child));
    }

    for (const Handle<Node>& child : children)
    {
        child->Remove();

        newNode->AddChild(child);
    }

    oldNode->Remove();

    return newNode;
}

// calls back with each reflected handle member of the node and of its components, and the object it's on
template <class Callback>
static void ForEachHandleMember(Node* node, Callback&& callback)
{
    BoxedValue target(MakeStrongRef(node));

    for (const Class* cls = node->InstanceClass(); cls != nullptr; cls = cls->GetParent())
    {
        for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
        {
            if (member.GetTypeInfo().IsHandleType() && CanGetAndSetMember(member))
            {
                callback(member, target);
            }
        }
    }

    Entity* entity = DynamicCast<Entity>(node);
    EntityManager* entityManager = entity != nullptr ? entity->GetEntityManager() : nullptr;

    if (entityManager == nullptr)
    {
        return;
    }

    Optional<const ComponentMap&> components = entityManager->GetAllComponents(entity);

    if (!components.HasValue())
    {
        return;
    }

    for (const auto& it : *components)
    {
        ComponentContainer* container = entityManager->TryGetContainer(it.first);

        if (container == nullptr || container->GetComponentInterface().IsEntityTag())
        {
            continue;
        }

        AnyRef componentRef = container->TryGetComponent(it.second);

        if (!componentRef.HasValue())
        {
            continue;
        }

        BoxedValue component(componentRef);

        for (const Property* property : container->GetComponentInterface().GetStruct()->GetProperties())
        {
            if (property->GetTypeInfo().IsHandleType() && CanGetAndSetMember(*property))
            {
                callback(*property, component);
            }
        }
    }
}

// reflected handles to a rebuilt node, on nodes and on their components, are pointed at its new node
static uint32 RetargetHandles(Node* node, const Array<RebuiltNode>& rebuiltNodes)
{
    uint32 numRetargeted = 0;

    ForEachHandleMember(node, [&rebuiltNodes, &numRetargeted](const IMember& member, BoxedValue& target)
        {
            const BoxedValue value = GetMemberValue(member, target);

            if (!value.Is<Handle<ObjectBase>>())
            {
                return;
            }

            if (const Handle<Node>* replacement = FindRebuilt(value.Get<Handle<ObjectBase>>().Get(), rebuiltNodes))
            {
                SetMemberValue(member, target, BoxedValue(*replacement));

                numRetargeted++;
            }
        });

    return numRetargeted;
}

// old nodes that refer to each other would keep each other, and the old build, alive
static void ClearHandlesToRebuiltNodes(Node* oldNode, const Array<RebuiltNode>& rebuiltNodes)
{
    ForEachHandleMember(oldNode, [&rebuiltNodes](const IMember& member, BoxedValue& target)
        {
            const BoxedValue value = GetMemberValue(member, target);

            if (value.Is<Handle<ObjectBase>>() && FindRebuilt(value.Get<Handle<ObjectBase>>().Get(), rebuiltNodes) != nullptr)
            {
                SetMemberValue(member, target, BoxedValue());
            }
        });
}

static Handle<Node> Remap(const Handle<Node>& node, const Array<RebuiltNode>& rebuiltNodes)
{
    const Handle<Node>* replacement = FindRebuilt(node.Get(), rebuiltNodes);

    return replacement != nullptr ? *replacement : node;
}

static void RemapEditorReferences(EditorSubsystem* subsystem, EditorProject* project, const Array<RebuiltNode>& rebuiltNodes)
{
    const Handle<Node> focusedNode = subsystem->GetFocusedNode();

    Array<Handle<Node>> selectedNodes = subsystem->GetSelectedNodes();

    for (Handle<Node>& selectedNode : selectedNodes)
    {
        selectedNode = Remap(selectedNode, rebuiltNodes);
    }

    subsystem->SetSelectedNodes(selectedNodes);

    if (FindRebuilt(focusedNode.Get(), rebuiltNodes) != nullptr)
    {
        subsystem->SetFocusedNode(Remap(focusedNode, rebuiltNodes), /* shouldSelectInOutline */ false);
    }

    if (g_editorState.IsValid())
    {
        Array<Handle<Node>> clipboardNodes = g_editorState->GetClipboardNodes();

        for (Handle<Node>& clipboardNode : clipboardNodes)
        {
            clipboardNode = Remap(clipboardNode, rebuiltNodes);
        }

        g_editorState->SetClipboardNodes(clipboardNodes);
    }

    // undo closures hold the old nodes and can't be remapped
    Proc<bool(EditorActionBase*)> removeAllActions = [](EditorActionBase*)
    {
        return true;
    };

    project->GetActionStack()->RemoveActions(removeAllActions);
}

} // namespace

EDITOR_API void RebuildRetiredModuleNodes(EditorSubsystem* subsystem, const Array<const Class*>& retiredClasses)
{
    const Handle<EditorProject>& project = subsystem->GetCurrentProject();

    if (!project.IsValid() || retiredClasses.Empty())
    {
        return;
    }

    if (subsystem->IsEditingPrefab())
    {
        subsystem->GetPrefabEditState()->Exit(/* apply */ true);
    }

    subsystem->ExitMeshEditMode(/* saveEdits */ true);
    subsystem->GetCsgState()->Exit(/* saveEdits */ true);

    const Array<Scene*> scenes = GetProjectScenes(project.Get());

    Array<Handle<Node>> retiredNodes;

    for (Scene* scene : scenes)
    {
        // components move to the new build's layouts first, since a rebuilt entity takes its old components with it
        scene->GetEntityManager()->SyncRuntimeComponentTypes();

        // parents come before their children
        for (Node* node : scene->GetRoot()->GetDescendantsArray())
        {
            if (DerivesFromRetiredClass(node->InstanceClass(), retiredClasses))
            {
                retiredNodes.PushBack(MakeStrongRef(node));
            }
        }
    }

    if (retiredNodes.Empty())
    {
        return;
    }

    Array<RebuiltNode> rebuiltNodes;

    for (const Handle<Node>& oldNode : retiredNodes)
    {
        const Class* oldClass = oldNode->InstanceClass();

        if (oldClass->IsDynamic())
        {
            HYP_LOG(Editor, Warning, "{} is a C# class deriving from a C++ game class; {} stays on the previous game module build", oldClass->GetName(), oldNode->GetName());

            continue;
        }

        const Class* newClass = GetClass(oldClass->GetTypeId());

        if (newClass == nullptr || newClass == oldClass)
        {
            HYP_LOG(Editor, Warning, "{} is gone from the new game module build; {} stays on the previous build until it's deleted", oldClass->GetName(), oldNode->GetName());

            continue;
        }

        Handle<Node> newNode = RebuildNode(oldNode, newClass);

        if (newNode.IsValid())
        {
            rebuiltNodes.PushBack({ oldNode, std::move(newNode) });
        }
    }

    if (rebuiltNodes.Empty())
    {
        return;
    }

    uint32 numRetargeted = 0;

    for (Scene* scene : scenes)
    {
        for (Node* node : scene->GetRoot()->GetDescendantsArray())
        {
            numRetargeted += RetargetHandles(node, rebuiltNodes);
        }
    }

    {
        // the old nodes are read and written through their own build's code
        ClassLookupOverrideScope lookupOverride(retiredClasses);

        for (const RebuiltNode& rebuiltNode : rebuiltNodes)
        {
            ClearHandlesToRebuiltNodes(rebuiltNode.oldNode.Get(), rebuiltNodes);
        }
    }

    RemapEditorReferences(subsystem, project.Get(), rebuiltNodes);

    // the inspector rebuilds its view of the selection the same way as for a script reload
    subsystem->OnScriptReloaded();

    HYP_LOG(Editor, Info, "Rebuilt {} node(s) of C++ game classes on the new build ({} reference(s) updated); undo history was cleared",
        rebuiltNodes.Size(), numRetargeted);
}

} // namespace Hyperion
