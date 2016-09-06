#include <iostream>
#include <string>

#include "glfw_engine.h"
#include "game.h"
#include "entity.h"
#include "asset/asset_manager.h"
#include "asset/text_loader.h"
#include "rendering/renderer.h"
#include "rendering/mesh.h"
#include "rendering/shader.h"
#include "rendering/environment.h"
#include "rendering/camera/ortho_camera.h"
#include "rendering/camera/perspective_camera.h"
#include "rendering/camera/fps_camera.h"
#include "rendering/texture.h"
#include "rendering/framebuffer.h"
#include "rendering/shaders/lighting_shader.h"
#include "rendering/shaders/post_shader.h"
#include "rendering/shaders/post/gamma_correct.h"
#include "rendering/shader_manager.h"
#include "rendering/shadow/shadow_mapping.h"
#include "rendering/shadow/pssm_shadow_mapping.h"
#include "util/shader_preprocessor.h"
#include "util/mesh_factory.h"
#include "util/aabb_factory.h"
#include "audio/audio_manager.h"
#include "audio/audio_source.h"
#include "audio/audio_control.h"
#include "animation/bone.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

/* Extra */
#include "rendering/skydome/skydome.h"
#include "terrain/noise_terrain/noise_terrain_control.h"

/* Physics */
#include "physics/physics_manager.h"
#include "physics/physics_control.h"
#include "physics/shapes/aabb_physics_object.h"

using namespace apex;

void DrawBoundingBox(Camera *camera, const BoundingBox &bb)
{
    auto corners = bb.GetCorners();

    // Cube 1x1x1, centered on origin
    GLfloat vertices[] = {
        corners[0].GetX(), corners[0].GetY(), corners[0].GetZ(), 1.0,
        corners[1].GetX(), corners[1].GetY(), corners[1].GetZ(), 1.0,
        corners[2].GetX(), corners[2].GetY(), corners[2].GetZ(), 1.0,
        corners[3].GetX(), corners[3].GetY(), corners[3].GetZ(), 1.0,
        corners[4].GetX(), corners[4].GetY(), corners[4].GetZ(), 1.0,
        corners[5].GetX(), corners[5].GetY(), corners[5].GetZ(), 1.0,
        corners[6].GetX(), corners[6].GetY(), corners[6].GetZ(), 1.0,
        corners[7].GetX(), corners[7].GetY(), corners[7].GetZ(), 1.0,
    };

    GLuint vbo_vertices;
    glGenBuffers(1, &vbo_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLushort elements[] = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    GLuint ibo_elements;
    glGenBuffers(1, &ibo_elements);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_elements);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,  // attribute
        4,                  // number of elements per vertex, here (x,y,z,w)
        GL_FLOAT,           // the type of each element
        GL_FALSE,           // take our values as-is
        0,                  // no extra data between each position
        0                   // offset of first element
        );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_elements);
    glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, 0);
    glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, (GLvoid*)(4 * sizeof(GLushort)));
    glDrawElements(GL_LINES, 8, GL_UNSIGNED_SHORT, (GLvoid*)(8 * sizeof(GLushort)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteBuffers(1, &vbo_vertices);
    glDeleteBuffers(1, &ibo_elements);
}

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    PssmShadowMapping *shadows;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> pbr_tex;
    std::shared_ptr<Mesh> debug_quad;

    BoundingBox bb1 = BoundingBox(-5, 5);

    double timer;
    double shadow_timer;
    bool scene_fbo_rendered;

    MyGame(const RenderWindow &window)
        : Game(window)
    {
        shadow_timer = 0.0f;
        timer = 0.15;
        scene_fbo_rendered = false;

        ShaderProperties defines;
        debug_quad = MeshFactory::CreateQuad();
        debug_quad->GetMaterial().alpha_blended = true;
        debug_quad->SetShader(ShaderManager::GetInstance()->GetShader<GammaCorrectShader>(defines));

        renderer = new Renderer();
        cam = new FpsCamera(inputmgr, &this->window, 70, 2.0, 250);
        fbo = new Framebuffer(window.width, window.height);
        shadows = new PssmShadowMapping(cam, 1, 20);
    }

    ~MyGame()
    {
        delete shadows;
        delete fbo;
        delete cam;
        delete renderer;
    }

    void Initialize()
    {
        Environment::GetInstance()->SetShadowsEnabled(true);

        AudioManager::GetInstance()->Initialize();

        ShaderProperties defines = {
            { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
            { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
        };
        shader = ShaderManager::GetInstance()->GetShader<LightingShader>(defines);

        top = std::make_shared<Entity>("top");

        auto torus = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/torus.obj");
        //torus->Scale(1.5f);
        torus->Move({ 0, 3, 5 });
        torus->GetChild(0)->GetRenderable()->SetShader(shader);
        torus->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 1.0f, 0.0f, 0.0f, 1.0f };

        auto torus_shape = new AABBPhysicsObject("red torus", 1.0, 1.0,
            AABBFactory::CreateEntityBoundingBox(torus));
        PhysicsManager::GetInstance()->AddPhysicsObject(torus_shape);
        torus->AddControl(std::make_shared<PhysicsControl>(torus_shape));

        auto audio_ctrl = std::make_shared<AudioControl>(
            AssetManager::GetInstance()->LoadFromFile<AudioSource>("res/sounds/cartoon001.wav"));
        torus->AddControl(audio_ctrl);
        audio_ctrl->GetSource()->SetLoop(true);
        audio_ctrl->GetSource()->Play();
        top->AddChild(torus);




        auto torus2 = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/torus2.obj");
        //torus2->Scale(1.5f);
        torus2->Move({ 0, 4, 5 });
        torus2->GetChild(0)->GetRenderable()->SetShader(shader);
        torus2->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 0.0f, 1.0f, 0.0f, 1.0f };

        auto torus2_shape = new AABBPhysicsObject("green torus", 0.6, 1.0,
            AABBFactory::CreateEntityBoundingBox(torus2));
        PhysicsManager::GetInstance()->AddPhysicsObject(torus2_shape);
        torus2->AddControl(std::make_shared<PhysicsControl>(torus2_shape));
        top->AddChild(torus2);

        auto torus3 = std::make_shared<Entity>();
        torus3->SetRenderable(torus2->GetChild(0)->GetRenderable());
        torus3->Move({ 0, 25, 15 });
        auto torus3_shape = new AABBPhysicsObject("small torus", 8.0, 1.0,
            AABBFactory::CreateEntityBoundingBox(torus3));
        torus3_shape->SetVelocity({ 0, -1, -6 });
        PhysicsManager::GetInstance()->AddPhysicsObject(torus3_shape);
        torus3->AddControl(std::make_shared<PhysicsControl>(torus3_shape));
        top->AddChild(torus3);



       // PhysicsManager::GetInstance()->AddPhysicsCollider(shape1);
       // PhysicsManager::GetInstance()->AddPhysicsCollider(shape2);

        /*auto dragger = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/ogrexml/dragger_Body.mesh.GetX()ml");
        dragger->Move(Vector3(3, -1.8f, 3));
        dragger->Scale(0.5);
        dragger->GetControl<SkeletonControl>(0)->SetLoop(true);
        dragger->GetControl<SkeletonControl>(0)->PlayAnimation(1, 3.0);
        top->AddChild(dragger);*/

        /*auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
        cube->GetChild(0)->GetRenderable()->SetShader(shader);
        cube->Scale(0.5);
        cube->SetName("cube");
        dragger->GetControl<SkeletonControl>(0)->GetBone("head")->AddChild(cube);
        */

        pbr_tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/grass2.jpg");

        auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/round_cube.obj");
        cube->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 0.0, 0.0, 1.0, 1.0 };
        cube->GetChild(0)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
           // {"DIFFUSE_MAP", true},
            {"SHADOWS", Environment::GetInstance()->ShadowsEnabled()},
            {"NUM_SPLITS", Environment::GetInstance()->NumCascades()}
        }));
        //cube->GetChild(0)->GetRenderable()->GetMaterial().diffuse_texture = pbr_tex;
        cube->Scale({ 5, 1, 5 });
        cube->Move({ 0, 0, 6 });
        cube->SetName("cube");

        auto cube_shape = new AABBPhysicsObject("cube", 0.0, 1.0,
            AABBFactory::CreateEntityBoundingBox(cube));
        Vector3 vel2 = { -1.0f, 1.0f, -0.8f };
        vel2.Normalize();
        vel2 *= 10;
        cube_shape->SetVelocity(vel2);
        PhysicsManager::GetInstance()->AddPhysicsObject(cube_shape);
        cube->AddControl(std::make_shared<PhysicsControl>(cube_shape));

        top->AddChild(cube);

        /*auto monkey = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/monkeyhq.obj");
        monkey->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = Vector4(0.0f, 0.9f, 0.2f, 1.0f);
        monkey->GetChild(0)->GetRenderable()->SetShader(shader);
        monkey->Move(Vector3(-3, 0, -3));
        monkey->SetName("monkey");
        top->AddChild(monkey);

        auto quad_node = std::make_shared<Entity>("quad");
        auto quad_mesh = MeshFactory::CreateQuad();
        quad_mesh->SetShader(shader);
        quad_node->SetRenderable(quad_mesh);
        quad_node->Scale(15);
        quad_node->Move(Vector3::UnitY() * -2);
        quad_node->Rotate(Quaternion(Vector3::UnitX(), MathUtil::PI / 2));
        top->AddChild(quad_node);*/

        top->AddControl(std::make_shared<SkydomeControl>(cam));
        //top->AddControl(std::make_shared<NoiseTerrainControl>(cam, -74));
    }

    void Logic(double dt)
    {
        // offset root node if camera is out of bounds
        const int bounds = 15;
        Vector3 campos(cam->GetTranslation());

        if (campos.GetX() >= bounds) {
            campos.SetX(0);
            top->SetLocalTranslation(top->GetLocalTranslation() - Vector3(bounds, 0, 0));
            cam->SetTranslation(campos);
        } else if (campos.GetX() <= -bounds) {
            campos.SetX(0);
            top->SetLocalTranslation(top->GetLocalTranslation() + Vector3(bounds, 0, 0));
            cam->SetTranslation(campos);
        }
        if (campos.GetZ() >= bounds) {
            campos.SetZ(0);
            top->SetLocalTranslation(top->GetLocalTranslation() - Vector3(0, 0, bounds));
            cam->SetTranslation(campos);
        } else if (campos.GetZ() <= -bounds) {
            campos.SetZ(0);
            top->SetLocalTranslation(top->GetLocalTranslation() + Vector3(0, 0, bounds));
            cam->SetTranslation(campos);
        }

        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        timer += dt;
        shadow_timer += dt;

        Environment::GetInstance()->GetSun().SetDirection(Vector3(sinf(timer * 0.005), cosf(timer * 0.005), 0.0f).Normalize());

        cam->Update(dt);
        top->Update(dt);

        PhysicsManager::GetInstance()->CheckCollisions();

        //top->GetChild(0)->Rotate(Quaternion(Vector3(1, 0, 0), dt));
        //top->GetChild(0)->SetLocalTranslation(Vector3(0, 0, (std::sin(timer) + 1.0f) * 5));
    }

    void Render()
    {
        renderer->FindRenderables(top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            if (shadow_timer >= 0.04) {
                Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
                shadow_dir.SetY(-1.0f);
                shadows->SetLightDirection(shadow_dir.Normalize());
                shadows->Render(renderer);
                shadow_timer = 0.0;
            }

            glDisable(GL_CULL_FACE);
        }

        //fbo->Use();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer->RenderAll(cam);

        //DrawBoundingBox(cam, bb1);

        renderer->ClearRenderables();

        /*debug_quad->GetMaterial().diffuse_texture = shadows->GetShadowMap();
        */

        /*fbo->End();

        debug_quad->GetMaterial().diffuse_texture = fbo->GetColorTexture();
        debug_quad->GetShader()->ApplyMaterial(debug_quad->GetMaterial());
        debug_quad->GetShader()->Use();
        debug_quad->Render();
        debug_quad->GetShader()->End();*/
    }
};

int main()
{
    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    auto *game = new MyGame(RenderWindow(1080, 720, "Apex Engine 5.0"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}