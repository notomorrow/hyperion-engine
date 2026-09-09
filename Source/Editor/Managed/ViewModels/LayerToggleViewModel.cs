using System;

namespace Hyperion.Editor.ViewModels
{
    public class LayerToggleViewModel : ViewModelBase
    {
        private readonly Action<string, bool> _onToggled;

        public string Name { get; }

        private bool _isActive;
        public bool IsActive
        {
            get => _isActive;
            set
            {
                if (SetProperty(ref _isActive, value))
                {
                    _onToggled(Name, value);
                }
            }
        }

        public LayerToggleViewModel(string name, bool isActive, Action<string, bool> onToggled)
        {
            Name = name;
            _isActive = isActive;
            _onToggled = onToggled;
        }
    }
}
