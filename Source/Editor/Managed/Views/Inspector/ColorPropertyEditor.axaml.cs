using System;
using System.ComponentModel;
using System.Globalization;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    using Color = Avalonia.Media.Color;

    public partial class ColorPropertyEditor : UserControl
    {
        private ColorViewModel? _openViewModel;
        private Color _originalColor;

        public ColorPropertyEditor()
        {
            InitializeComponent();

            Swatch.PointerPressed += OnSwatchPointerPressed;

            if (FlyoutBase.GetAttachedFlyout(Swatch) is PopupFlyoutBase flyout)
            {
                flyout.Opening += OnFlyoutOpening;
                flyout.Closed += OnFlyoutClosed;
            }

            Picker.ColorChanging += OnPickerColorChanging;
            Picker.ColorCommitted += OnPickerColorCommitted;

            OriginalSwatch.PointerPressed += OnOriginalSwatchPointerPressed;

            HexTextBox.KeyDown += OnHexTextBoxKeyDown;
            HexTextBox.LostFocus += OnHexTextBoxLostFocus;
        }

        private void OnSwatchPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (!e.GetCurrentPoint(Swatch).Properties.IsLeftButtonPressed || DataContext is not ColorViewModel { IsReadOnly: false })
            {
                return;
            }

            FlyoutBase.ShowAttachedFlyout(Swatch);
            e.Handled = true;
        }

        private void OnFlyoutOpening(object? sender, EventArgs e)
        {
            if (DataContext is not ColorViewModel viewModel)
            {
                return;
            }

            _openViewModel = viewModel;
            _openViewModel.PropertyChanged += OnViewModelPropertyChanged;

            FlyoutContent.DataContext = viewModel;

            _originalColor = viewModel.PickerColor;
            OriginalSwatch.Background = new SolidColorBrush(_originalColor);

            ShowColor(viewModel.PickerColor);
        }

        private void OnFlyoutClosed(object? sender, EventArgs e)
        {
            if (_openViewModel == null)
            {
                return;
            }

            // Closed mid-drag (e.g. the window lost focus) - keep what was picked rather than leave a preview behind.
            if (Picker.IsDragging)
            {
                _openViewModel.CommitPickerColor(Picker.Color);
            }

            _openViewModel.PropertyChanged -= OnViewModelPropertyChanged;
            _openViewModel = null;
        }

        // Undo/redo or the text boxes can change the value while the flyout is open.
        private void OnViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName != nameof(ColorViewModel.PickerColor) || _openViewModel == null || Picker.IsDragging)
            {
                return;
            }

            ShowColor(_openViewModel.PickerColor);
        }

        private void OnPickerColorChanging(object? sender, EventArgs e)
        {
            _openViewModel?.PreviewPickerColor(Picker.Color);

            HexTextBox.Text = FormatHex(Picker.Color);
        }

        private void OnPickerColorCommitted(object? sender, EventArgs e)
        {
            _openViewModel?.CommitPickerColor(Picker.Color);
        }

        private void OnOriginalSwatchPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (_openViewModel == null)
            {
                return;
            }

            ShowColor(_originalColor);
            _openViewModel.CommitPickerColor(_originalColor);

            e.Handled = true;
        }

        private void OnHexTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return)
            {
                CommitHexText();
                e.Handled = true;
            }
        }

        private void OnHexTextBoxLostFocus(object? sender, RoutedEventArgs e)
        {
            CommitHexText();
        }

        private void CommitHexText()
        {
            if (_openViewModel == null)
            {
                return;
            }

            if (!TryParseHex(HexTextBox.Text, Picker.Color.A, out Color color))
            {
                HexTextBox.Text = FormatHex(Picker.Color);
                return;
            }

            if (color == Picker.Color)
            {
                HexTextBox.Text = FormatHex(color);
                return;
            }

            ShowColor(color);
            _openViewModel.CommitPickerColor(color);
        }

        private void ShowColor(Color color)
        {
            Picker.Color = color;

            if (!HexTextBox.IsFocused)
            {
                HexTextBox.Text = FormatHex(color);
            }
        }

        private static string FormatHex(Color color)
        {
            return $"{color.R:X2}{color.G:X2}{color.B:X2}{color.A:X2}";
        }

        // RRGGBB keeps the current alpha; RRGGBBAA sets it too.
        private static bool TryParseHex(string? text, byte currentAlpha, out Color color)
        {
            color = default;

            string hex = (text ?? string.Empty).Trim().TrimStart('#');

            if ((hex.Length != 6 && hex.Length != 8) || !uint.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint packed))
            {
                return false;
            }

            if (hex.Length == 6)
            {
                packed = packed << 8 | currentAlpha;
            }

            color = Color.FromArgb((byte)packed, (byte)(packed >> 24), (byte)(packed >> 16), (byte)(packed >> 8));

            return true;
        }
    }
}
