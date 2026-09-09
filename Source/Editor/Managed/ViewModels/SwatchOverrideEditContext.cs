using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class SwatchOverrideEditContext
    {
        public static Entity? CurrentEntity { get; set; }

        public static bool OverrideModeActive { get; set; }

        /// <summary>Name of the World's active swatch, or null when unknown.</summary>
        public static string? ActiveSwatchName { get; set; }

        public static void Reset()
        {
            CurrentEntity = null;
            ActiveSwatchName = null;
        }
    }
}
