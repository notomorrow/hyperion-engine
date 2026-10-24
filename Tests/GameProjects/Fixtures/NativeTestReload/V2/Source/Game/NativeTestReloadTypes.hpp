#pragma once

#include <Scene/Entity.hpp>
#include <Scene/System.hpp>
#include <Scene/Subsystem.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {

// The second build of the reload test types: fields are added and retyped, and NativeTestObsoleteEntity is removed

HYP_STRUCT(Component)
struct NativeTestReloadComponent
{
    HYP_STRUCT_BODY(NativeTestReloadComponent);

    HYP_FIELD(Property = "Speed", Serialize)
    float speed = 1.0f;

    HYP_FIELD(Property = "Tag", Serialize)
    String tag;

    HYP_FIELD(Property = "Boost", Serialize)
    float boost = 2.0f;
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

    HYP_FIELD(Property = "Mood", Serialize)
    String mood = "calm";
};

HYP_CLASS()
class NativeTestReloadEntity : public Entity
{
    HYP_OBJECT_BODY(NativeTestReloadEntity);

public:
    HYP_FIELD(Property = "Health", Serialize)
    double health = 100.0;

    HYP_FIELD(Property = "Armor", Serialize)
    int32 armor = 5;

    HYP_FIELD(Property = "Label", Serialize)
    String label;

    HYP_FIELD(Property = "Buddy")
    Handle<Node> buddy;

    HYP_FIELD(Property = "Partner")
    Handle<NativeTestReloadEntity> partner;

    HYP_FIELD(Property = "Note", Serialize)
    Handle<NativeTestReloadNote> note;
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
