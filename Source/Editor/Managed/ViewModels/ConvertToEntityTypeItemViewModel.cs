using System;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ConvertToEntityTypeItemViewModel : ViewModelBase
    {
        private readonly Node _node;

        public ConvertToEntityTypeItemViewModel(string className, Node node)
        {
            ClassName = className ?? throw new ArgumentNullException(nameof(className));
            _node = node ?? throw new ArgumentNullException(nameof(node));

            ConvertCommand = new RelayCommand(Execute);
        }

        public string ClassName { get; }

        public ICommand ConvertCommand { get; }

        private void Execute()
        {
            if (!_node.IsValid)
            {
                return;
            }

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                new Name("EditorCommandConvertNodeToEntityType"),
                _node.NativeAddress.ToString(),
                ClassName);
        }
    }
}
