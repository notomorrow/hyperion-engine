using System;

namespace Hyperion
{
    [ClassBinding(Name = "TerrainSculptMode")]
    public enum TerrainSculptMode : byte
    {
        Raise = 0,
        Lower = 1,
        PaintSplat = 2
    }

    [ClassBinding(Name = "TerrainSculpting")]
    public class TerrainSculpting : ObjectBase
    {
        public bool IsEnabled => this.IsEnabled();

        public float Radius => this.GetRadius();

        public float Strength => this.GetStrength();

        public TerrainSculptMode Mode => this.GetMode();

        public int PaintLayer => this.GetPaintLayer();
    }
}
