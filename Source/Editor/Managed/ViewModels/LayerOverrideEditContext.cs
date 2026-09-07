using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class LayerOverrideEditContext
    {
        public static Entity? CurrentEntity { get; set; }

        public static bool OverrideModeActive { get; set; }

        /// <summary>Name of the World's active layer, or null when unknown.</summary>
        public static string? ActiveLayerName { get; set; }

        public static void Reset()
        {
            CurrentEntity = null;
            ActiveLayerName = null;
        }
    }
}
