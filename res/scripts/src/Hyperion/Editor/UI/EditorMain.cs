
using System;
using System.IO;
using System.Linq;
using Hyperion;

namespace Hyperion
{
    namespace Editor
    {
        public class TestEditorTask : LongRunningEditorTask
        {
            public TestEditorTask()
            {
            }

            public override void Cancel()
            {
                Logger.Log(LogType.Info, "Cancel task");
            }

            public override bool IsCompleted()
            {
                return false;
            }

            public override void Process()
            {
                Logger.Log(LogType.Info, "Process task! testing");
            }
        }

        public class FPSCounterDebugOverlay : EditorDebugOverlayBase
        {
            private static readonly List<KeyValuePair<int, Color>> fpsColors = new List<KeyValuePair<int, Color>>
            {
                new KeyValuePair<int, Color>(30, new Color(1.0f, 0.0f, 0.0f, 1.0f)),
                new KeyValuePair<int, Color>(60, new Color(1.0f, 1.0f, 0.0f, 1.0f)),
                new KeyValuePair<int, Color>(int.MaxValue, new Color(0.0f, 1.0f, 0.0f, 1.0f))
            };

            private UIText? tpsTextElement;
            private UIText? memoryUsageTextElement;
            private UIText? fpsTextElement;
            private UIText? countersTextElement;
            private float deltaAccumGame = 0.0f;
            private int numTicksGame = 0;
            private World world;

            public FPSCounterDebugOverlay(World world)
            {
                this.world = world;
            }

            public override UIObject CreateUIObject(UIObject spawnParent)
            {
                UIListView panel = spawnParent.Spawn<UIListView>(new Name("FPSCounterDebugOverlay_Panel"), new Vec2i(0, 0), new UIObjectSize(100, UIObjectSize.Percent, 0, UIObjectSize.Auto));
                panel.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));

                UIListView renderListView = spawnParent.Spawn<UIListView>(new Name("FPSCounterDebugOverlay_RenderListView"), new Vec2i(0, 0), new UIObjectSize(100, UIObjectSize.Percent, 15, UIObjectSize.Pixel));
                renderListView.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));
                renderListView.SetOrientation(UIListViewOrientation.Horizontal);
                renderListView.SetTextSize(8);

                UIText fpsTextElement = renderListView.Spawn<UIText>(new Name("FPSCounterDebugOverlay_FPS"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                fpsTextElement.SetText("Render:");
                fpsTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                fpsTextElement.SetPadding(new Vec2i(5, 5));
                renderListView.AddChildUIObject(fpsTextElement);
                this.fpsTextElement = fpsTextElement;

                UIText countersTextElement = renderListView.Spawn<UIText>(new Name("FPSCounterDebugOverlay_Counters"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                countersTextElement.SetText("draw calls: 0, Tris: 0");
                countersTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                countersTextElement.SetPadding(new Vec2i(5, 5));
                renderListView.AddChildUIObject(countersTextElement);
                this.countersTextElement = countersTextElement;

                UIText tpsTextElement = renderListView.Spawn<UIText>(new Name("FPSCounterDebugOverlay_TPS"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                tpsTextElement.SetText("Game:");
                tpsTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                tpsTextElement.SetPadding(new Vec2i(5, 5));
                renderListView.AddChildUIObject(tpsTextElement);
                this.tpsTextElement = tpsTextElement;

                UIText memoryUsageTextElement = renderListView.Spawn<UIText>(new Name("FPSCounterDebugOverlay_MemoryUsage"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                memoryUsageTextElement.SetText(".NET Memory Usage: 0 MB");
                memoryUsageTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                memoryUsageTextElement.SetPadding(new Vec2i(5, 5));
                renderListView.AddChildUIObject(memoryUsageTextElement);
                this.memoryUsageTextElement = memoryUsageTextElement;

                panel.AddChildUIObject(renderListView);

                return panel;
            }

            public override Name GetName()
            {
                return new Name("FPSCounterDebugOverlay");
            }

            public override bool IsEnabled()
            {
                return true;
            }

            public override void Update(float delta)
            {
                deltaAccumGame += delta;
                numTicksGame++;

                if (deltaAccumGame >= 1.0f)
                {
                    if (tpsTextElement != null)
                    {
                        int ticksPerSecondGame = (int)(1.0f / (deltaAccumGame / (float)numTicksGame));

                        ((UIText)tpsTextElement).SetText(string.Format("Game: {0} ticks/sec", ticksPerSecondGame));
                        tpsTextElement.SetTextColor(GetFPSColor(ticksPerSecondGame));
                    }

                    if (memoryUsageTextElement != null)
                    {
                        long memoryUsage = GC.GetTotalMemory(false) / 1024 / 1024;

                        ((UIText)memoryUsageTextElement).SetText(string.Format(".NET Memory Usage: {0} MB", memoryUsage));
                    }

                    deltaAccumGame = 0.0f;
                    numTicksGame = 0;
                }

                var renderStats = world.GetRenderStats();

                if (fpsTextElement != null)
                {
                    ((UIText)fpsTextElement).SetText(string.Format("Render: {0} frames/sec, {1:0.00} ms/frame (avg: {2:0.00}, min: {3:0.00}, max: {4:0.00})",
                        (int)renderStats.framesPerSecond, renderStats.millisecondsPerFrame,
                        renderStats.millisecondsPerFrameAvg, renderStats.millisecondsPerFrameMin, renderStats.millisecondsPerFrameMax));
                    fpsTextElement.SetTextColor(GetFPSColor((int)renderStats.framesPerSecond));
                }

                if (countersTextElement != null)
                {
                    ((UIText)countersTextElement).SetText(string.Format("draw calls: {0}, Tris: {1}",
                        renderStats.counts.drawCalls, renderStats.counts.triangles));
                }
            }

            private static Color GetFPSColor(int fps)
            {
                for (int i = 0; i < fpsColors.Count; i++)
                {
                    if (fps <= fpsColors[i].Key)
                    {
                        return fpsColors[i].Value;
                    }
                }

                return fpsColors[fpsColors.Count - 1].Value;
            }
        }

        public class EditorMain : UIEventHandler
        {
            FirstPersonCameraController? firstPersonCameraController;

            Scene? testScene;

            public override void Init(Entity entity)
            {
                base.Init(entity);

                Logger.Log(LogType.Info, "EditorMain Init");

                testScene = new Scene();
                testScene.SetName(new Name("TestScene"));
                testScene.SetFlags(SceneFlags.Foreground);

                Camera camera = new Camera();
                camera.SetName(new Name("TestCamera"));

                firstPersonCameraController = new FirstPersonCameraController();
                // firstPersonCameraController.SetMode(FirstPersonCameraControllerMode.MouseLocked);
                camera.AddCameraController(firstPersonCameraController);

                var cameraNode = new Node();
                cameraNode.SetName("TestCameraNode");

                var cameraEntity = testScene.GetEntityManager().AddEntity();
                testScene.GetEntityManager().AddComponent<CameraComponent>(cameraEntity, new CameraComponent { Camera = camera });
                cameraNode.SetEntity(cameraEntity);

                testScene.GetRoot().AddChild(cameraNode);

                // // test
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();
                editorSubsystem.AddDebugOverlay(new FPSCounterDebugOverlay(World));
            }

            ~EditorMain()
            {
                Logger.Log(LogType.Info, "EditorMain finalizer");
            }

            public override void Destroy()
            {
                Logger.Log(LogType.Info, "EditorMain Destroy()");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                editorSubsystem.RemoveDebugOverlay(new Name("TestEditorDebugOverlay", weak: true));
            }

            public override void OnPlayStart()
            {
                Logger.Log(LogType.Info, "OnPlayStart");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();
                editorSubsystem.GetScene().RemoveFromWorld();

                // firstPersonCameraController = new FirstPersonCameraController();
                // editorSubsystem.GetScene().GetPrimaryCamera().AddCameraController(firstPersonCameraController);

                // firstPersonCameraController.SetMode(FirstPersonCameraControllerMode.MouseLocked);
                ((Scene)testScene).GetRoot().AddChild(editorSubsystem.GetScene().GetRoot());
                ((Scene)testScene).AddToWorld(World);
            }

            public override void OnPlayStop()
            {
                Logger.Log(LogType.Info, "OnPlayStop");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                ((Scene)testScene).RemoveFromWorld();
                editorSubsystem.GetScene().SetRoot(((Scene)testScene).GetRoot().GetChild(0));

                editorSubsystem.GetScene().AddToWorld(World);

                // if (!editorSubsystem.GetScene().GetPrimaryCamera().RemoveCameraController(firstPersonCameraController))
                // {
                //     Logger.Log(LogType.Error, "Failed to remove camera controller!");

                //     return;
                // }

                firstPersonCameraController = null;

                Logger.Log(LogType.Info, "Camera controller removed");
            }

            public UIEventHandlerResult SaveClicked()
            {
                Logger.Log(LogType.Info, "Save clicked");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                EditorProject currentProject = editorSubsystem.GetCurrentProject();

                if (currentProject == null)
                {
                    Logger.Log(LogType.Error, "No project loaded; cannot save");

                    return UIEventHandlerResult.Error;
                }

                Result saveResult = currentProject.Save();

                if (!saveResult.IsValid)
                {
                    Logger.Log(LogType.Error, "Failed to save project");

                    return UIEventHandlerResult.Error;
                }

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult SimulateClicked()
            {
                // Test: Force GC
                GC.Collect();

                World world = Scene.GetWorld();
                
                if (world.GetGameState().Mode == GameStateMode.Simulating)
                {
                    Logger.Log(LogType.Info, "Stop simulation");

                    world.StopSimulating();
                    return UIEventHandlerResult.Ok;
                }

                Logger.Log(LogType.Info, "Start simulation");

                world.StartSimulating();

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddPointLight()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                var light = new Light();
                light.SetLightType(LightType.Point);
                light.SetPosition(new Vec3f(0.0f, 5.0f, 0.0f));
                light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                light.SetRadius(30.0f);
                light.SetIntensity(20.0f);

                var lightEntity = editorSubsystem.GetScene().GetEntityManager().AddEntity();

                editorSubsystem.GetScene().GetEntityManager().AddComponent<LightComponent>(lightEntity, new LightComponent {
                    Light = light
                });

                editorSubsystem.GetScene().GetEntityManager().AddComponent<ShadowMapComponent>(lightEntity, new ShadowMapComponent {
                });

                var lightNode = new Node();
                lightNode.SetName(editorSubsystem.GetScene().GetUniqueNodeName("PointLight"));
                lightNode.SetEntity(lightEntity);
                lightNode.SetWorldTranslation(new Vec3f(0.0f, 5.0f, 0.0f));

                editorSubsystem.GetScene().GetRoot().AddChild(lightNode);

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddDirectionalLight()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                var light = new Light();
                light.SetLightType(LightType.Directional);
                light.SetPosition(new Vec3f(-0.5f, 0.5f, 0.0f).Normalize());
                light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                light.SetIntensity(5.0f);

                var lightEntity = editorSubsystem.GetScene().GetEntityManager().AddEntity();

                var lightNode = new Node();
                lightNode.SetName(editorSubsystem.GetScene().GetUniqueNodeName("DirectionalLight"));
                lightNode.SetEntity(lightEntity);
                lightNode.SetWorldTranslation(new Vec3f(-0.5f, 0.5f, 0.0f).Normalize());

                editorSubsystem.GetScene().GetRoot().AddChild(lightNode);

                editorSubsystem.GetScene().GetEntityManager().AddComponent<LightComponent>(lightEntity, new LightComponent {
                    Light = light
                });

                editorSubsystem.GetScene().GetEntityManager().AddComponent<ShadowMapComponent>(lightEntity, new ShadowMapComponent {
                });

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddSpotLight()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                var light = new Light();
                light.SetLightType(LightType.Spot);
                light.SetPosition(new Vec3f(0.0f, 0.0f, 0.0f));
                light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                light.SetRadius(10.0f);
                light.SetIntensity(5.0f);
                light.SetSpotAngles(new Vec2f(30.0f * MathF.PI / 180.0f, 60.0f * MathF.PI / 180.0f));

                var lightEntity = editorSubsystem.GetScene().GetEntityManager().AddEntity();

                editorSubsystem.GetScene().GetEntityManager().AddComponent<LightComponent>(lightEntity, new LightComponent {
                    Light = light
                });

                var lightNode = new Node();
                lightNode.SetName(editorSubsystem.GetScene().GetUniqueNodeName("SpotLight"));
                lightNode.SetEntity(lightEntity);
                lightNode.SetWorldTranslation(new Vec3f(0.0f, 0.0f, 0.0f));

                editorSubsystem.GetScene().GetRoot().AddChild(lightNode);

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddAreaRectLight()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                var light = new Light();
                light.SetLightType(LightType.AreaRect);
                light.SetPosition(new Vec3f(0.0f, 0.0f, 0.0f));
                light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                light.SetIntensity(5.0f);
                light.SetAreaSize(new Vec2f(2.0f, 2.0f));

                var lightEntity = editorSubsystem.GetScene().GetEntityManager().AddEntity();

                editorSubsystem.GetScene().GetEntityManager().AddComponent<LightComponent>(lightEntity, new LightComponent {
                    Light = light
                });

                var lightNode = new Node();
                lightNode.SetName(editorSubsystem.GetScene().GetUniqueNodeName("AreaRectLight"));
                lightNode.SetEntity(lightEntity);
                lightNode.SetWorldTranslation(new Vec3f(0.0f, 0.0f, 0.0f));

                editorSubsystem.GetScene().GetRoot().AddChild(lightNode);

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddReflectionProbe()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                var envProbeEntity = editorSubsystem.GetScene().GetEntityManager().AddEntity();

                editorSubsystem.GetScene().GetEntityManager().AddComponent<ReflectionProbeComponent>(envProbeEntity, new ReflectionProbeComponent {
                    Dimensions = new Vec2u(256, 256)
                });
                editorSubsystem.GetScene().GetEntityManager().AddComponent<BoundingBoxComponent>(envProbeEntity, new BoundingBoxComponent {
                    LocalAABB = new BoundingBox(new Vec3f(-15.0f, 0.0f, -15.0f), new Vec3f(15.0f, 15.0f, 15.0f)),
                    WorldAABB = new BoundingBox(new Vec3f(-15.0f, 0.0f, -15.0f), new Vec3f(15.0f, 15.0f, 15.0f))
                });
                editorSubsystem.GetScene().GetEntityManager().AddComponent<TransformComponent>(envProbeEntity, new TransformComponent {
                    Transform = new Transform()
                });

                var envProbeNode = new Node();
                envProbeNode.SetName(editorSubsystem.GetScene().GetUniqueNodeName("ReflectionProbe"));
                envProbeNode.SetEntity(envProbeEntity);
                envProbeNode.SetWorldTranslation(new Vec3f(0.0f, 5.0f, 0.0f));

                editorSubsystem.GetScene().GetRoot().AddChild(envProbeNode);

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult UndoClicked()
            {
                Logger.Log(LogType.Info, "Undo clicked");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                if (!editorSubsystem.GetActionStack().CanUndo())
                {
                    Logger.Log(LogType.Info, "Cannot undo, stack is empty");

                    return UIEventHandlerResult.Ok;
                }

                editorSubsystem.GetActionStack().Undo();

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult RedoClicked()
            {
                Logger.Log(LogType.Info, "Redo clicked");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");
                    
                    return UIEventHandlerResult.Error;
                }

                if (!editorSubsystem.GetActionStack().CanRedo())
                {
                    Logger.Log(LogType.Info, "Cannot redo, stack is empty");

                    return UIEventHandlerResult.Ok;
                }

                editorSubsystem.GetActionStack().Redo();

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddNodeClicked()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Ok;
                }

                var node = new Node();
                node.SetName("New Node");

                editorSubsystem.GetActionStack().Push(new EditorAction(
                    new Name("AddNewNode"),
                    () =>
                    {
                        editorSubsystem.GetScene().GetRoot().AddChild(node);
                        editorSubsystem.SetFocusedNode(node);
                    },
                    () =>
                    {
                        node.Remove();

                        if (editorSubsystem.GetFocusedNode() == node)
                        {
                            editorSubsystem.SetFocusedNode(null);
                        }
                    }
                ));

                // @TODO Focus on node in scene view

                return UIEventHandlerResult.Ok;
            }
        }
    }
}