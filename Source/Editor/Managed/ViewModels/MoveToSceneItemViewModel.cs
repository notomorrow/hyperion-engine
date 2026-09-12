using System;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class MoveToSceneItemViewModel : ViewModelBase
    {
        private readonly Scene _scene;
        private readonly Node _node;

        public MoveToSceneItemViewModel(Scene scene, Node node)
        {
            _scene = scene ?? throw new ArgumentNullException(nameof(scene));
            _node = node ?? throw new ArgumentNullException(nameof(node));

            MoveToSceneCommand = new RelayCommand(Execute);
        }

        public string SceneName => _scene.Name.ToString();

        public ICommand MoveToSceneCommand { get; }

        private void Execute()
        {
            if (!_node.IsValid || !_scene.IsValid)
            {
                return;
            }

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                new Name("EditorCommandMoveNodeToScene"),
                _node.NativeAddress.ToString(),
                _scene.NativeAddress.ToString());
        }
    }
}
