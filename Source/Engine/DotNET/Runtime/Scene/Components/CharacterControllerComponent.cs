using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="CharacterMovementSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterMovementSettings
    {
        public float MoveSpeed = 15.0f;
        public float SprintSpeed = 25.0f;
        public float GroundAcceleration = 12.0f;
        public float AirAcceleration = 3.0f;
        public float Friction = 8.0f;
        public float StopSpeed = 2.5f;
        public float StepHeight = 0.35f;
        public float MaxSlopeAngle = 45.0f;
        public float SprintAcceleration = 7.0f;
        public float SprintTurnRate = 140.0f;
        public float TurnSpeedLoss = 1.5f;
        public float BrakeDeceleration = 22.0f;

        [MarshalAs(UnmanagedType.I1)]
        public bool OrientToMovement = false;

        public float TurnRate = 360.0f;

        public CharacterMovementSettings()
        {
        }
    }

    [ClassBinding(Name="CharacterJumpSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterJumpSettings
    {
        public float Speed = 4.9f;
        public float CutGravityMultiplier = 2.2f;
        public float ApexGravityMultiplier = 0.85f;
        public float FallGravityMultiplier = 1.8f;
        public float FallSpeed = 55.0f;
        public float CoyoteTime = 0.15f;
        public float BufferTime = 0.15f;
        public float WindupTime = 0.1f;

        public CharacterJumpSettings()
        {
        }
    }

    [ClassBinding(Name="CharacterPushSettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterPushSettings
    {
        public float MassLimit = 350.0f;
        public float MaxSpeed = 1.5f;
        public float SpeedScale = 1.0f;
        public float PredictionReleaseDelay = 0.25f;
        public float MinGroundSupportMass = 20.0f;

        public CharacterPushSettings()
        {
        }
    }

    [ClassBinding(Name="CharacterShadowBodySettings")]
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterShadowBodySettings
    {
        public float MaxSpeed = 60.0f;
        public float TeleportDistance = 0.5f;

        public CharacterShadowBodySettings()
        {
        }
    }

    [ClassBinding(Name="CharacterControllerComponent")]
    [StructLayout(LayoutKind.Explicit, Size = Layout.Size, Pack = 16)]
    public ref struct CharacterControllerComponent : IComponent
    {
        private static class Layout
        {
#if HYP_WEB
            public const int InputHandler = 4, PhysicsHandle = 8, Base = 16, Size = 192;
#else
            public const int InputHandler = 8, PhysicsHandle = 16, Base = 32, Size = 208;
#endif
        }

        [FieldOffset(0)]
        public Handle<PhysicsShape> Shape;
        [FieldOffset(Layout.InputHandler)]
        public Handle<InputHandlerBase> InputHandler;
        [FieldOffset(Layout.PhysicsHandle)]
        public SharedPtr PhysicsHandle; // SharedPtr<void> - internal, do not access directly

        [FieldOffset(Layout.Base)]
        public Vec3f ViewDirection = new Vec3f(0.0f, 0.0f, 1.0f);
        [FieldOffset(Layout.Base + 16)]
        public Vec3f Translation;
        [FieldOffset(Layout.Base + 32)]
        public Vec3f Heading;

        [FieldOffset(Layout.Base + 48)]
        public CharacterMovementSettings Movement = new CharacterMovementSettings();
        [FieldOffset(Layout.Base + 104)]
        public CharacterJumpSettings Jump = new CharacterJumpSettings();
        [FieldOffset(Layout.Base + 136)]
        public CharacterPushSettings Push = new CharacterPushSettings();
        [FieldOffset(Layout.Base + 156)]
        public CharacterShadowBodySettings ShadowBody = new CharacterShadowBodySettings();

        [FieldOffset(Layout.Base + 164)]
        [MarshalAs(UnmanagedType.I1)]
        public bool IsOnGround;

        [FieldOffset(Layout.Base + 168)]
        public float JumpWindupRemaining;

        public CharacterControllerComponent()
        {
        }

        public void Dispose()
        {
            Shape.Dispose();
            InputHandler.Dispose();

            //Hmm... @TODO address me
            ///PhysicsHandle.Dispose();
        }
    }
}
