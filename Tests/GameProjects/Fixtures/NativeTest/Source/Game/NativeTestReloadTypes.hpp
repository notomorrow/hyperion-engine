#pragma once

#include <Scene/Entity.hpp>
#include <Scene/System.hpp>
#include <Scene/Subsystem.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {

// Game types that live objects are made of when the reload tests rebuild the game; Fixtures/NativeTestReload/V2 changes them

HYP_STRUCT(Component)
struct NativeTestReloadComponent
{
    HYP_STRUCT_BODY(NativeTestReloadComponent);

    HYP_FIELD(Property = "Speed", Serialize)
    float speed = 1.0f;

    HYP_FIELD(Property = "Tag", Serialize)
    String tag;
};

HYP_CLASS()
class NativeTestReloadNote final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestReloadNote);

public:
    HYP_FIELD(Property = "Text", Serialize)
    String text;

    HYP_FIELD(Property = "Priority", Serialize)
    int32 priority = 1;
};

HYP_CLASS()
class NativeTestReloadEntity : public Entity
{
    HYP_OBJECT_BODY(NativeTestReloadEntity);

public:
    HYP_FIELD(Property = "Health", Serialize)
    int32 health = 100;

    HYP_FIELD(Property = "Label", Serialize)
    String label;

    HYP_FIELD(Property = "Buddy")
    Handle<Node> buddy;

    HYP_FIELD(Property = "Partner")
    Handle<NativeTestReloadEntity> partner;

    HYP_FIELD(Property = "Note", Serialize)
    Handle<NativeTestReloadNote> note;
};

HYP_CLASS()
class NativeTestObsoleteEntity final : public Entity
{
    HYP_OBJECT_BODY(NativeTestObsoleteEntity);
};

HYP_CLASS(NoScriptBindings)
class NativeTestReloadSystem final : public SystemBase
{
    HYP_OBJECT_BODY(NativeTestReloadSystem);

public:
    HYP_FIELD(Property = "Counter", Serialize)
    int32 counter = 0;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override
    {
    }

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<NativeTestReloadComponent, ComponentAccess::READ> {}
        };
    }
};

HYP_CLASS(NoScriptBindings)
class NativeTestReloadSubsystem final : public Subsystem
{
    HYP_OBJECT_BODY(NativeTestReloadSubsystem);

public:
    HYP_FIELD(Property = "Ticks")
    int32 ticks = 0;

    virtual void OnAddedToWorld() override
    {
    }

    virtual void OnRemovedFromWorld() override
    {
    }

    virtual void Update(float delta) override
    {
    }
};

} // namespace Hyperion
