#include "game.h"
#include "rendering/camera/fps_camera.h"

namespace hyperion {

Game::Game(const RenderWindow &window) 
    : m_input_manager(new InputManager),
      m_ui_manager(new UIManager(m_input_manager)),
      m_renderer(new Renderer(window)),
      m_scene(new Node("Scene")),
      m_ui(new Node("UI")),
      m_scene_manager(SceneManager::GetInstance())
{
    m_camera = new FpsCamera(
        GetInputManager(),
        &m_renderer->GetRenderWindow(),
        m_renderer->GetRenderWindow().GetScaledWidth(),
        m_renderer->GetRenderWindow().GetScaledHeight(),
        65.0f,
        0.05f,
        250.0f
    );

    m_scene->SetOctant(make_non_owning(m_scene_manager->GetOctree()));

    // just till we have a better way of manually traversing the ui node
    //m_ui->SetOctant(make_non_owning(m_scene_manager->GetOctree()));
}

Game::~Game()
{
    delete m_camera;
    delete m_input_manager;
    delete m_ui_manager;
    delete m_renderer;
}

void Game::Update(double dt)
{
    m_ui_manager->Update(dt);

    Logic(dt);

    m_camera->Update(dt);

    m_scene->Update(dt);
    m_ui->Update(dt);
}

void Game::PreRender()
{
    m_scene_manager->GetOctree()->UpdateVisibilityState(
        Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN,
        m_camera->GetFrustum()
    );
}

void Game::Render()
{
    PreRender();

    OnRender();

    m_renderer->Render(m_camera, Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN);

    PostRender();
}

void Game::PostRender()
{
    m_scene->ClearPendingRemoval();
    m_ui->ClearPendingRemoval();

    m_renderer->ClearRenderables(Octree::VisibilityState::CameraType::VIS_CAMERA_MAIN);
}

} // namespace hyperion
