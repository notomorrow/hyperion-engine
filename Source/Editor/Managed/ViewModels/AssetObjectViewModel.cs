using System;
using System.Collections.ObjectModel;
using System.Threading;
using Avalonia.Threading;
using Hyperion;
using Lucide.Avalonia;

namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectViewModel : ViewModelBase
    {
        private readonly AssetDesc _assetDesc;
        private readonly AssetBucketViewModel? _bucket;
        private readonly string? _typeName;
        private readonly DateTime? _dateModified;

        public AssetDesc AssetDesc => _assetDesc;
        public AssetBucketViewModel? Bucket => _bucket;

        public string DisplayName => _assetDesc.Name.ToString();

        /// <summary>Asset class name extracted from the manifest (e.g. "MeshAsset"), or null if unavailable.</summary>
        public string? TypeName => _typeName;

        /// <summary>Last-write time of the manifest file on disk, or null if unavailable.</summary>
        public DateTime? DateModified => _dateModified;

        // @TODO Centralize this somewhere (same for scene hierarchy icons)
        public LucideIconKind IconKind => _typeName switch
        {
            "Mesh" or "MeshAsset"                  => LucideIconKind.Box,
            "Material"                             => LucideIconKind.Paintbrush, // @TODO Better icon for Material
            "Texture" or "TextureAsset"            => LucideIconKind.Image,
            "DirectionalLight"                     => LucideIconKind.Sun,
            "PointLight"                           => LucideIconKind.Lightbulb,
            "SpotLight"                            => LucideIconKind.Spotlight,
            "AreaRectLight"                        => LucideIconKind.RectangleHorizontal,
            "Camera"                               => LucideIconKind.Video,
            "ReflectionProbe"                      => LucideIconKind.Orbit,
            "ParticleVolume"                       => LucideIconKind.Sparkles,
            "InstancedMeshProxy"                   => LucideIconKind.SquaresUnite,
            "Skeleton"                             => LucideIconKind.Bone,
            "Animation" or "AnimationTrack"        => LucideIconKind.Film,
            "Scene" or "World"                     => LucideIconKind.Globe,
            "LightmapVolume"                       => LucideIconKind.Box,
            "FogVolume"                            => LucideIconKind.Cloudy,
            "Entity"                               => LucideIconKind.Shapes,
            "Node"                                 => LucideIconKind.Circle,
            "Shader" or "ShaderBundle"             => LucideIconKind.CodeXml,
            "FontAtlas"                            => LucideIconKind.Type,
            "Sound" or "Audio"                     => LucideIconKind.Volume2,
            "PhysicsShape"                         => LucideIconKind.Triangle,
            "Script"                               => LucideIconKind.FileCode,
            _ when _bucket?.BucketIndex >= 0       => LucideIconKind.File,
            _                                      => LucideIconKind.Circle,
        };

        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private int _isRefreshingActions;

        public AssetObjectViewModel(AssetDesc assetDesc, AssetBucketViewModel? bucket = null, string? typeName = null, DateTime? dateModified = null)
        {
            _assetDesc = assetDesc;
            _bucket = bucket;
            _typeName = typeName;
            _dateModified = dateModified;
        }

        /// <summary>Resolves the live asset object on the sim thread and repopulates <see cref="Actions"/> from its EditorAction-attributed methods (e.g. "Regenerate Mipmaps" on Texture).</summary>
        public void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_bucket == null)
            {
                return;
            }

            if (Interlocked.Exchange(ref _isRefreshingActions, 1) != 0)
            {
                return;
            }

            uint bucketIndex = _bucket.BucketIndex;
            Name assetName = _assetDesc.Name;

            _ = EngineManager.PostToSimThread(() =>
            {
                AssetObject? obj = AssetManager.Instance.AssetRegistry.GetAsset(bucketIndex, assetName);

                Dispatcher.UIThread.Post(() =>
                {
                    _isRefreshingActions = 0;

                    Actions.Clear();

                    foreach (InspectorActionViewModel actionVm in InspectorActionsHelper.GetActions(obj))
                    {
                        Actions.Add(actionVm);
                    }

                    HasActions = Actions.Count > 0;
                });
            });
        }
    }
}
