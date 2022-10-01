#include <ui/UIScene.hpp>
#include <camera/OrthoCamera.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

UIScene::UIScene()
    : EngineComponentBase()
{
}

UIScene::~UIScene()
{
    Teardown();
}

void UIScene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    m_scene = engine->CreateHandle<Scene>(
        engine->CreateHandle<Camera>(new OrthoCamera(
            1024, 1024,
            -1, 1,
            -1, 1,
            -1, 1
        ))
    );

    engine->InitObject(m_scene);

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        m_scene.Reset();
    });

    // for (auto &ui_object : m_ui_objects) {
    //     engine->InitObject(ui_object);
    // }
}

void UIScene::Update(Engine *engine, GameCounter::TickUnit delta)
{
    m_scene->Update(engine, delta, false);
}

#if 0
bool UIScene::Add(Handle<UIObject> &&ui_object)
{
    if (!ui_object || !ui_object->GetID()) {
        return false;
    }

    auto result = m_ui_objects.Insert(ui_object->GetID(), std::move(ui_object));

    if (!result.second) {
        return false;
    }

    auto insert_result = result.first;

    if (IsInitCalled()) {
        GetEngine()->InitObject(insert_result->second);
    }

    return true;
}

bool UIScene::Add(const Handle<UIObject> &ui_object)
{
    if (!ui_object || !ui_object->GetID()) {
        return false;
    }

    auto result = m_ui_objects.Insert(ui_object->GetID(), ui_object);

    if (!result.second) {
        return false;
    }

    auto insert_result = result.first;

    if (IsInitCalled()) {
        GetEngine()->InitObject(insert_result->second);
    }

    return true;
}

bool UIScene::Remove(const UIObject::ID &id)
{
    if (!id) {
        return false;
    }

    auto it = m_ui_objects.Find(id);

    if (it == m_ui_objects.End()) {
        return false;
    }

    return m_ui_objects.Erase(it);
}
#endif

} // namespace hyperion::v2