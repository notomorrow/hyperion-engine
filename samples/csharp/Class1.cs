﻿using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

using Hyperion;

public class TestUIScript : UIEventHandler
{
    public override async void Init(Entity entity)
    {
        base.Init(entity);

        UIObject.OriginAlignment = UIObjectAlignment.TopLeft;

        var tmpEntity = Scene.EntityManager.AddEntity();
        var meshComponentId = Scene.EntityManager.AddComponent<MeshComponent>(tmpEntity, new MeshComponent
        {
            Mesh = new Mesh(
                new List<Vertex>
                {
                    new Vertex(new Vec3f(0, 0, 0), new Vec2f(0, 0), new Vec3f(0, 0, 1)),
                    new Vertex(new Vec3f(1, 0, 0), new Vec2f(1, 0), new Vec3f(0, 0, 1)),
                    new Vertex(new Vec3f(1, 1, 0), new Vec2f(1, 1), new Vec3f(0, 0, 1)),
                    new Vertex(new Vec3f(0, 1, 0), new Vec2f(0, 1), new Vec3f(0, 0, 1))
                },
                new List<uint>
                {
                    0, 1, 2,
                    2, 3, 0
                }
            ),
            Material = new Material()
        });

        var meshComponent = Scene.EntityManager.GetComponent<MeshComponent>(tmpEntity);
        var mesh = meshComponent.Mesh;

        
        var assetBatch = new AssetBatch();
        assetBatch.Add("test_model", "models/house.obj");

        var assetMap = await assetBatch.Load();

        Scene.Root.AddChild(assetMap["test_model"].GetNode());
    }

    public override void Destroy()
    {
        Logger.Log(LogType.Info, "Destroy UI script");
    }

    public override UIEventHandlerResult OnClick()
    {
        Logger.Log(LogType.Info, "Typeof this UI Object: {0}", UIObject.GetType().Name);

        // Logger.Log(LogType.Info, "Object RefCountedPtr address: {0}", UIObject.RefCountedPtr.Address);
        // Logger.Log(LogType.Info, "Object name: {0}", UIObject.Name.HashCode);
        // Vec2i offset = new Vec2i(10, 10);
        // Logger.Log(LogType.Info, "OnClick for custom UI component {0} {1}", offset.X, offset.Y);

        // Logger.Log(LogType.Info, "Current position: {0}, {1}", UIObject.Position.X, UIObject.Position.Y);

        // UIObject.Position += offset;

        return UIEventHandlerResult.Ok;
    }
}

public class TestScript : Script
{
    public override void Init(Entity entity)
    {
        base.Init(entity);

        // var lightEntity = Scene.EntityManager.AddEntity();
        // Scene.EntityManager.AddComponent<LightComponent>(lightEntity, new LightComponent
        // {
        //     Light = new Light(
        //         LightType.Point,
        //         new Vec3f(0f, 0.5f, 0f),
        //         new Color(1, 0, 0, 1),
        //         1.0f,
        //         25.0f
        //     )
        // });

        // Scene.EntityManager.AddComponent<TransformComponent>(lightEntity, new TransformComponent
        // {
        //     transform = new Transform
        //     {
        //         Translation = new Vec3f(0, 0.5f, 0),
        //         Scale = new Vec3f(1, 1, 1),
        //         Rotation = new Quaternion(0, 0, 0, 1)
        //     }
        // });

        // // Scene.EntityManager.AddComponent<ShadowMapComponent>(lightEntity, new ShadowMapComponent { });

        // Logger.Log(LogType.Info, "Init script, added light entity: " + lightEntity.ID);
    }

    public override void Destroy()
    {
        // Logger.Log(LogType.Info, "Destroy a script");
    }

    // public override void Update(float deltaTime)
    // {
    //     Logger.Log(LogType.Info, "Update a script");
    // }
}

public class TestGame : Game
{
    public override async void Init()
    {
        var assetBatch = new AssetBatch();
        assetBatch.Add("test_model", "models/house.obj");

        var assetMap = await assetBatch.Load();

        Scene.Root.AddChild(assetMap["test_model"].GetNode());

        // var lightNode = Scene.Root.AddChild();
        // var lightEntity = Scene.EntityManager.AddEntity();
        // Scene.EntityManager.AddComponent<LightComponent>(lightEntity, new LightComponent
        // {
        //     Light = new Light(
        //         LightType.Point,
        //         new Vec3f(0, 3, 0),
        //         new Color(1, 0, 0, 1),
        //         50.0f,
        //         50.0f
        //     )
        // });

        // Scene.EntityManager.AddComponent<TransformComponent>(lightEntity, new TransformComponent
        // {
        //     transform = new Transform
        //     {
        //         Translation = new Vec3f(0, 3, 0),
        //         Scale = new Vec3f(1, 1, 1),
        //         Rotation = new Quaternion(0, 0, 0, 1)
        //     }
        // });

        // lightNode.Entity = lightEntity;
    }

    public override void Update(float deltaTime)
    {
    }
}

public class MyClass
{
    public int InstanceMethod()
    {
        Logger.Log(LogType.Info, "Hello from C#, instance method called");
        return 9999;
    }

    public static void MyMethod()
    {
        // NativeInterop.Initialize("build/HyperionRuntime.dll");

#if false
        Scene scene = new Scene();
        var sceneID = scene.ID;

        Logger.Log(LogType.Info, "Hello from C#, scene ID: " + sceneID);

        // print address of engine instance to test
        Logger.Log(LogType.Info, "Hello from C#, world ID: " + Engine.Instance.World.ID);

        Node root = scene.Root;

        Node myNode = root.AddChild();
        myNode.LocalTranslation += new Vec3f(1, 2, 3);
        myNode.LocalTranslation += new Vec3f(1, 2, 3);
        myNode.LocalTranslation += new Vec3f(1, 2, 3);
        myNode.Name = "Foo bar baggins";

        Logger.Log(LogType.Info, "Hello from C#, myNode name: " + myNode.Name);
        Logger.Log(LogType.Info, "Hello from C#, myNode local transform: " + myNode.LocalTransform);
        Logger.Log(LogType.Info, "Hello from C#, myNode world transform: " + myNode.WorldTransform);

        // test EntityManager
        EntityManager entityManager = scene.EntityManager;

        Entity entity = entityManager.AddEntity();

        // add transform component
        var transformComponentId = scene.EntityManager.AddComponent<TransformComponent>(entity, new TransformComponent
        {
            transform = new Transform
            {
                Translation = new Vec3f(1, 2, 3),
                Scale = new Vec3f(1, 1, 1),
                Rotation = new Quaternion(0, 0, 0, 1)
            }
        });

        var transformComponent0 = entityManager.GetComponent<TransformComponent>(entity);
        transformComponent0.transform.Translation += new Vec3f(10, 10, 10);

        Logger.Log(LogType.Info, "Hello from C#, entity ID: {0}", entity.ID);
        Logger.Log(LogType.Info, "Hello from C#, entity has transform component: {0}", entityManager.HasComponent<TransformComponent>(entity));
        Logger.Log(LogType.Info, "Hello from C#, transformComponentId: {0}", transformComponentId);

        // try getting the transform component
        var transformComponent1 = entityManager.GetComponent<TransformComponent>(entity);
        Logger.Log(LogType.Info, "Hello from C#, transformComponent: {0}", transformComponent1.transform);
        myNode.Entity = entity;

        // move node; should move entity
        myNode.LocalTranslation += new Vec3f(20, 20, 20);

        // test entity translation after moving node
        var transformComponent2 = entityManager.GetComponent<TransformComponent>(entity);
        Logger.Log(LogType.Info, "Hello from C#, transformComponent: {0}", transformComponent2.transform);

        var meshComponentId = scene.EntityManager.AddComponent<MeshComponent>(entity, new MeshComponent
        {
            Mesh = new Mesh(
                new List<Vertex>
                {
                    new Vertex(new Vec3f(0, 0, 0), new Vec2f(0, 0), new Vec3f(0, 0, 1)),
                    new Vertex(new Vec3f(1, 0, 0), new Vec2f(1, 0), new Vec3f(0, 0, 1)),
                    new Vertex(new Vec3f(1, 1, 0), new Vec2f(1, 1), new Vec3f(0, 0, 1)),
                    new Vertex(new Vec3f(0, 1, 0), new Vec2f(0, 1), new Vec3f(0, 0, 1))
                },
                new List<uint>
                {
                    0, 1, 2,
                    2, 3, 0
                }
            ),
            Material = new Material()
        });

        var meshComponent = entityManager.GetComponent<MeshComponent>(entity);
        Logger.Log(LogType.Info, "Hello from C#, meshComponentId: {0}", meshComponentId);
        Logger.Log(LogType.Info, "Hello from C#, mesh component: {0}", meshComponent);
        meshComponent.Mesh.Init();
        meshComponent.Material[MaterialKey.Albedo] = new MaterialParameter(1.0f, 0.0f, 0.0f, 1.0f);
        meshComponent.Material[MaterialKey.Roughness] = new MaterialParameter(0.5f);
        Logger.Log(LogType.Info, "Hello from C#, material param albedo type: {0}", meshComponent.Material[MaterialKey.Albedo].Type);
        Logger.Log(LogType.Info, "Hello from C#, material param albedo: {0}", meshComponent.Material[MaterialKey.Albedo]);

        meshComponent.Material.Textures[TextureKey.AlbedoMap] = new Texture();
        meshComponent.Material.Textures[TextureKey.AlbedoMap].Init();
        Logger.Log(LogType.Info, "Hello from C#, material texture albedo map: {0}", meshComponent.Material.Textures[TextureKey.AlbedoMap].ID);

        scene.EntityManager.AddComponent<VisibilityStateComponent>(entity, new VisibilityStateComponent { });

        bool childRemoved = root.RemoveChild(myNode);
        Logger.Log(LogType.Info, "Hello from C#, childRemoved: {0}", childRemoved);
        // see if this breaks:
        Logger.Log(LogType.Info, "Hello from C#, myNode name: {0}", myNode.Name);
#endif
    }
}
