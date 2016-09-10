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
#include "physics/collision_box.h"
#include "physics/collision_sphere.h"
#include "physics/complex_collision_detector.h"
#include "physics/gravity_force.h"
#include "physics/physics_control.h"
#include "physics/shapes/aabb_physics_object.h"

using namespace apex;

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    PssmShadowMapping *shadows;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Entity> cube, torus, torus3;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> pbr_tex;
    std::shared_ptr<Mesh> debug_quad;
    std::shared_ptr<RigidBody> test_body, test_body2, test_body3;

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

        torus = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/torus.obj");
        //torus->Scale(1.5f);
        //torus->Move({ 0, 3, 5 });
        torus->GetChild(0)->GetRenderable()->SetShader(shader);
        torus->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 1.0f, 0.0f, 0.0f, 1.0f };

        auto torus_shape = new AABBPhysicsObject("red torus", 1.0, 1.0,
            AABBFactory::CreateEntityBoundingBox(torus));
        //PhysicsManager::GetInstance()->AddPhysicsObject(torus_shape);
        //torus->AddControl(std::make_shared<PhysicsControl>(torus_shape));

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
        //PhysicsManager::GetInstance()->AddPhysicsObject(torus2_shape);
        //torus2->AddControl(std::make_shared<PhysicsControl>(torus2_shape));
        top->AddChild(torus2);

        torus3 = std::make_shared<Entity>();
        torus3->SetRenderable(torus2->GetChild(0)->GetRenderable());
        torus3->Move({ 0, 25, 15 });
        auto torus3_shape = new AABBPhysicsObject("small torus", 8.0, 1.0,
            AABBFactory::CreateEntityBoundingBox(torus3));
        torus3_shape->SetVelocity({ 0, -1, -6 });
        //PhysicsManager::GetInstance()->AddPhysicsObject(torus3_shape);
        //torus3->AddControl(std::make_shared<PhysicsControl>(torus3_shape));
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

        //pbr_tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/grass2.jpg");

        cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/round_cube.obj");
        cube->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 0.15f, 0.3f, 1.0f, 1.0f };
        cube->GetChild(0)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
           // {"DIFFUSE_MAP", true},
            {"SHADOWS", Environment::GetInstance()->ShadowsEnabled()},
            {"NUM_SPLITS", Environment::GetInstance()->NumCascades()}
        }));
        //cube->GetChild(0)->GetRenderable()->GetMaterial().diffuse_texture = pbr_tex;
       // cube->Scale({ 5, 1, 5 });
       // cube->Move({ 0, 0, 6 });
        cube->SetName("cube");

        auto cube_shape = new AABBPhysicsObject("cube", 0.0, 1.0,
            AABBFactory::CreateEntityBoundingBox(cube));
        //PhysicsManager::GetInstance()->AddPhysicsObject(cube_shape);
        //cube->AddControl(std::make_shared<PhysicsControl>(cube_shape));

        top->AddChild(cube);

        // add a test RigidBody.
        test_body = std::make_shared<RigidBody>(0.5);
        test_body->position = Vector3(0, 0, 0);
        test_body->SetLinearDamping(0.02);
        test_body->SetAngularDamping(0.02);
        test_body->can_sleep = false;
        test_body->SetAwake(true);
        //test_body->acceleration = GravityForce::GRAVITY;
        test_body->ClearAccumulators();
        test_body->CalculateDerivedData();
        PhysicsManager::GetInstance()->RegisterBody(test_body);

        // add another
        test_body2 = std::make_shared<RigidBody>(2.0);
        test_body2->position = Vector3(2, 20, 0);
        test_body2->acceleration = GravityForce::GRAVITY;
       // test_body2->SetInverseInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1), 2.0));
        test_body2->SetLinearDamping(0.2);
        test_body2->SetAngularDamping(0.2);
        test_body2->can_sleep = false;
        test_body2->SetAwake(true);
        test_body2->ClearAccumulators();
        test_body2->CalculateDerivedData();
        PhysicsManager::GetInstance()->RegisterBody(test_body2);

        // add another
        test_body3 = std::make_shared<RigidBody>(8.0);
        test_body3->position = Vector3(1.2,-10, 0);
        test_body3->acceleration = Vector3(0, 10, 0);
        test_body3->SetLinearDamping(0.3);
        test_body3->SetAngularDamping(0.3);
        test_body3->can_sleep = false;
        test_body3->SetAwake(true);
        test_body3->ClearAccumulators();
        test_body3->CalculateDerivedData();
        PhysicsManager::GetInstance()->RegisterBody(test_body3);


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


        const double theta = 0.25f;

        // test collision
        apex::CollisionBox box;
        box.m_half_size = 1.0;
        box.m_body = test_body.get();
        box.m_body->Integrate(dt * theta);
        box.CalculateInternals();

        apex::CollisionBox box2;
        box2.m_half_size = 1.0;
        box2.m_body = test_body2.get();
        box2.m_body->Integrate(dt * theta);
        box2.CalculateInternals();

        apex::CollisionBox box3;
        box3.m_half_size = 1.0;
        box3.m_body = test_body3.get();
        box3.m_body->Integrate(dt * theta);
        box3.CalculateInternals();

        Contact contact;
        contact.m_bodies = { box.m_body, box2.m_body };

        Contact contact2;
        contact2.m_bodies = { box.m_body, box3.m_body };

        Contact contact3;
        contact3.m_bodies = { box2.m_body, box3.m_body };

        // test contacts and fill data
        CollisionData data; 
        data.m_friction = 0.9;
        data.m_restitution = 0.9;
        data.m_tolerance = 0.9;
        data.m_contacts = { &contact, &contact2, &contact3 };
        data.m_contact_index = 0;
        data.m_contact_count = 3;
        data.m_contacts_left = 3;

        apex::ComplexCollisionDetector::SphereAndSphere(box, box2, data);
        apex::ComplexCollisionDetector::SphereAndSphere(box, box3, data);
        apex::ComplexCollisionDetector::SphereAndSphere(box2, box3, data);
        
        //PhysicsManager::GetInstance()->Begin();
        //PhysicsManager::GetInstance()->RunPhysics(dt * theta);

        // resolve contacts
        std::array<Contact, MAX_CONTACTS> contact_array = { contact, contact2, contact3 };
        PhysicsManager::GetInstance()->resolver->SetNumIterations(3*4);
        PhysicsManager::GetInstance()->resolver->ResolveContacts(contact_array, 3, dt * theta);

        /*if (shadow_timer > 0.05) {
            std::cout << "test_body->position = " << test_body->position << "\n";
            std::cout << "test_body2->position = " << test_body2->position << "\n\n";
            shadow_timer = 0.0;
        }*/

        cube->SetLocalTranslation(test_body->position);
        cube->SetLocalRotation(test_body->orientation);
        torus->SetLocalTranslation(test_body2->position);
        torus->SetLocalRotation(test_body2->orientation);
        torus3->SetLocalTranslation(test_body3->position);
        torus3->SetLocalRotation(test_body3->orientation);
       // std::cout << "test_body->rotation = " << test_body->rotation << "\n";


        //PhysicsManager::GetInstance()->CheckCollisions();

        //top->GetChild(0)->Rotate(Quaternion(Vector3(1, 0, 0), dt));
        //top->GetChild(0)->SetLocalTranslation(Vector3(0, 0, (std::sin(timer) + 1.0f) * 5));
    

        top->Update(dt);
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