#include <Game/NativeEngineTests.hpp>
#include <Game/NativeTestTypes.hpp>
#include <Game/NativeTestReloadTypes.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion::tests {

namespace {

// engine code that reloading a game module relies on, run in the game's own build

Handle<NativeTestCounter> CreateCounter()
{
    Handle<NativeTestCounter> counter = MakeHandle<NativeTestCounter>();
    counter->baseValue = 3;
    counter->count = 7;
    counter->label = "seven";
    counter->scratch = 9;

    return counter;
}

Handle<NativeTestWideCounter> CopyToWideCounter(const Handle<NativeTestCounter>& counter, const Class* stopAtClass, EnumFlags<CopyMembersFlags> flags)
{
    Handle<NativeTestWideCounter> wideCounter = MakeHandle<NativeTestWideCounter>();

    BoxedValue source { Handle<ObjectBase>(counter) };
    BoxedValue target { Handle<ObjectBase>(wideCounter) };

    CopyMembersByName(NativeTestCounter::StaticClass(), source, NativeTestWideCounter::StaticClass(), target, stopAtClass, flags);

    return wideCounter;
}

Handle<Scene> CreateTestScene()
{
    Handle<Scene> scene = MakeHandle<Scene>(NAME("NativeEngineTestScene"));
    InitObject(scene);

    return scene;
}

void CopyMembersByName_RetypesMatchingFields(GameTestContext& context)
{
    Handle<NativeTestWideCounter> wideCounter = CopyToWideCounter(CreateCounter(), NativeTestCounterBase::StaticClass(), CopyMembersFlags::None);

    context.Check(wideCounter->count == 7.0, HYP_FORMAT("Count: expected 7.0, got {}", wideCounter->count));
    context.Check(wideCounter->label == "seven", HYP_FORMAT("Label: expected 'seven', got '{}'", wideCounter->label));
    context.Check(wideCounter->baseValue == 0, HYP_FORMAT("BaseValue is on the stop class, so it should stay 0, got {}", wideCounter->baseValue));
    context.Check(wideCounter->scratch == 0, HYP_FORMAT("Scratch is transient, so it should stay 0, got {}", wideCounter->scratch));
}

void CopyMembersByName_CopiesBaseClassMembers(GameTestContext& context)
{
    Handle<NativeTestWideCounter> wideCounter = CopyToWideCounter(CreateCounter(), nullptr, CopyMembersFlags::None);

    context.Check(wideCounter->baseValue == 3, HYP_FORMAT("BaseValue: expected 3, got {}", wideCounter->baseValue));
}

void CopyMembersByName_CopiesTransientWhenAsked(GameTestContext& context)
{
    Handle<NativeTestWideCounter> wideCounter = CopyToWideCounter(CreateCounter(), nullptr, CopyMembersFlags::IncludeTransient);

    context.Check(wideCounter->scratch == 9, HYP_FORMAT("Scratch: expected 9, got {}", wideCounter->scratch));
}

void Node_InsertChild_PlacesAtIndex(GameTestContext& context)
{
    Handle<Scene> scene = CreateTestScene();
    const Handle<Node>& root = scene->GetRoot();

    for (const char* name : { "First", "Second", "Third" })
    {
        Handle<Node> node = MakeHandle<Node>();
        node->SetName(CreateNameFromDynamicString(name));

        root->AddChild(node);
    }

    Handle<Node> inserted = MakeHandle<Node>();
    inserted->SetName(NAME("Inserted"));

    root->InsertChild(inserted, 1);

    Handle<Node> appended = MakeHandle<Node>();
    appended->SetName(NAME("Appended"));

    root->InsertChild(appended, 100);

    const char* expectedOrder[] = { "First", "Inserted", "Second", "Third", "Appended" };

    if (!context.Check(root->GetChildren().Size() == std::size(expectedOrder), HYP_FORMAT("expected {} children, got {}", std::size(expectedOrder), root->GetChildren().Size())))
    {
        return;
    }

    for (uint32 index = 0; index < std::size(expectedOrder); index++)
    {
        context.Check(root->GetChildren()[index]->GetName() == CreateNameFromDynamicString(expectedOrder[index]),
            HYP_FORMAT("child {}: expected {}, got {}", index, expectedOrder[index], root->GetChildren()[index]->GetName()));
    }

    context.Check(inserted->FindSelfIndex() == 1, HYP_FORMAT("Inserted's FindSelfIndex: expected 1, got {}", inserted->FindSelfIndex()));
}

void GameComponent_AddsAndReadsBack(GameTestContext& context)
{
    Handle<Scene> scene = CreateTestScene();

    Handle<Entity> entity = MakeHandle<Entity>();
    scene->GetRoot()->AddChild(entity);

    NativeTestReloadComponent added;
    added.speed = 4.5f;
    added.tag = "fast";

    entity->AddComponent<NativeTestReloadComponent>(std::move(added));

    const NativeTestReloadComponent* component = entity->TryGetComponent<NativeTestReloadComponent>();

    if (!context.Check(component != nullptr, "the entity has no NativeTestReloadComponent"))
    {
        return;
    }

    context.Check(component->speed == 4.5f, HYP_FORMAT("Speed: expected 4.5, got {}", component->speed));
    context.Check(component->tag == "fast", HYP_FORMAT("Tag: expected 'fast', got '{}'", component->tag));
}

void GameEntity_CloneAsKeepsFieldsAndComponents(GameTestContext& context)
{
    Handle<Scene> scene = CreateTestScene();

    Handle<Node> buddy = MakeHandle<Node>();
    buddy->SetName(NAME("Buddy"));
    scene->GetRoot()->AddChild(buddy);

    Handle<NativeTestReloadEntity> original = MakeHandle<NativeTestReloadEntity>();
    original->SetName(NAME("Original"));
    original->health = 42;
    original->label = "alpha";
    original->buddy = buddy;
    scene->GetRoot()->AddChild(original);

    NativeTestReloadComponent added;
    added.speed = 2.0f;
    added.tag = "cloned";

    original->AddComponent<NativeTestReloadComponent>(std::move(added));

    Handle<Node> clonedNode = original->CloneAs(NativeTestReloadEntity::StaticClass(), /* cloneChildren */ false);

    if (!context.Check(clonedNode.IsValid() && clonedNode->InstanceClass() == NativeTestReloadEntity::StaticClass(), "CloneAs didn't make a NativeTestReloadEntity"))
    {
        return;
    }

    scene->GetRoot()->AddChild(clonedNode);

    const NativeTestReloadEntity& cloned = static_cast<const NativeTestReloadEntity&>(*clonedNode);

    context.Check(cloned.GetName() == NAME("Original"), HYP_FORMAT("name: expected Original, got {}", cloned.GetName()));
    context.Check(cloned.health == 42, HYP_FORMAT("Health: expected 42, got {}", cloned.health));
    context.Check(cloned.label == "alpha", HYP_FORMAT("Label: expected 'alpha', got '{}'", cloned.label));
    context.Check(cloned.buddy == buddy, "Buddy doesn't refer to the original's buddy");

    const NativeTestReloadComponent* component = cloned.TryGetComponent<NativeTestReloadComponent>();

    if (!context.Check(component != nullptr, "the clone has no NativeTestReloadComponent"))
    {
        return;
    }

    context.Check(component->speed == 2.0f && component->tag == "cloned",
        HYP_FORMAT("component: expected (2, 'cloned'), got ({}, '{}')", component->speed, component->tag));
}

void MathUtil_RoundAtRuntime(GameTestContext& context)
{
    struct RoundCase
    {
        double value;
        int decimalPlaces;
        double expected;
    };

    // read through a volatile so the runtime path runs rather than the constant-evaluated one
    static const RoundCase s_cases[] = {
        { 2.345, 2, 2.35 },
        { 2.344, 2, 2.34 },
        { -1.25, 1, -1.3 },
        { 1234.5, -2, 1200.0 },
        { 0.5, 0, 1.0 }
    };

    for (const RoundCase& roundCase : s_cases)
    {
        volatile double value = roundCase.value;

        const double rounded = MathUtil::Round(double(value), roundCase.decimalPlaces);

        context.Check(MathUtil::Abs(rounded - roundCase.expected) < 1e-9,
            HYP_FORMAT("Round({}, {}): expected {}, got {}", roundCase.value, roundCase.decimalPlaces, roundCase.expected, rounded));
    }
}

} // namespace

const Array<GameTestDefinition>& GetNativeEngineTests()
{
    static const Array<GameTestDefinition> s_tests {
        { "CopyMembersByName_RetypesMatchingFields", g_writePhase, &CopyMembersByName_RetypesMatchingFields },
        { "CopyMembersByName_CopiesBaseClassMembers", g_writePhase, &CopyMembersByName_CopiesBaseClassMembers },
        { "CopyMembersByName_CopiesTransientWhenAsked", g_writePhase, &CopyMembersByName_CopiesTransientWhenAsked },
        { "Node_InsertChild_PlacesAtIndex", g_writePhase, &Node_InsertChild_PlacesAtIndex },
        { "GameComponent_AddsAndReadsBack", g_writePhase, &GameComponent_AddsAndReadsBack },
        { "GameEntity_CloneAsKeepsFieldsAndComponents", g_writePhase, &GameEntity_CloneAsKeepsFieldsAndComponents },
        { "MathUtil_RoundAtRuntime", g_writePhase, &MathUtil_RoundAtRuntime }
    };

    return s_tests;
}

} // namespace Hyperion::tests
