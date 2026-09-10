using System;
using System.Collections.ObjectModel;
using System.Threading;
using Avalonia.Threading;
using Hyperion;

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

        // @TODO also, dont just switch on typename -- switch on class -- we dont want to add a new entry for EVERY subclass !
        public string IconKind => _typeName != null
            ? AssetIconHelper.FromTypeName(_typeName)
            : "File";

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
