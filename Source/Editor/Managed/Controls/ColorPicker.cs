using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;

namespace Hyperion.Editor.Controls
{
    using Color = Avalonia.Media.Color;

    public class ColorPicker : Control
    {
        public static readonly StyledProperty<Color> ColorProperty =
            AvaloniaProperty.Register<ColorPicker, Color>(nameof(Color), Colors.White);

        public static readonly StyledProperty<bool> ShowAlphaProperty =
            AvaloniaProperty.Register<ColorPicker, bool>(nameof(ShowAlpha), true);

        private const double StripWidth = 14.0;
        private const double StripSpacing = 8.0;
        private const double CheckerSize = 5.0;

        private enum DragRegion
        {
            None,
            SaturationValue,
            Hue,
            Alpha
        }

        private static readonly IBrush HueBrush = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
            GradientStops =
            {
                new GradientStop(Color.FromRgb(255, 0, 0), 0.0 / 6.0),
                new GradientStop(Color.FromRgb(255, 255, 0), 1.0 / 6.0),
                new GradientStop(Color.FromRgb(0, 255, 0), 2.0 / 6.0),
                new GradientStop(Color.FromRgb(0, 255, 255), 3.0 / 6.0),
                new GradientStop(Color.FromRgb(0, 0, 255), 4.0 / 6.0),
                new GradientStop(Color.FromRgb(255, 0, 255), 5.0 / 6.0),
                new GradientStop(Color.FromRgb(255, 0, 0), 6.0 / 6.0)
            }
        };

        private static readonly IBrush WhiteToClearBrush = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(1, 0, RelativeUnit.Relative),
            GradientStops =
            {
                new GradientStop(Colors.White, 0.0),
                new GradientStop(Color.FromArgb(0, 255, 255, 255), 1.0)
            }
        };

        private static readonly IBrush ClearToBlackBrush = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
            GradientStops =
            {
                new GradientStop(Color.FromArgb(0, 0, 0, 0), 0.0),
                new GradientStop(Colors.Black, 1.0)
            }
        };

        private static readonly IBrush CheckerLightBrush = new SolidColorBrush(Color.FromRgb(204, 204, 204));
        private static readonly IBrush CheckerDarkBrush = new SolidColorBrush(Color.FromRgb(140, 140, 140));
        private static readonly IPen OutlinePen = new Pen(new SolidColorBrush(Color.FromArgb(160, 0, 0, 0)), 1.0);
        private static readonly IPen MarkerOuterPen = new Pen(Brushes.Black, 3.0);
        private static readonly IPen MarkerInnerPen = new Pen(Brushes.White, 1.5);

        private double _hue;
        private double _saturation;
        private double _value = 1.0;
        private double _alpha = 1.0;

        private DragRegion _dragRegion = DragRegion.None;
        private bool _isSettingColorFromInput;

        static ColorPicker()
        {
            AffectsRender<ColorPicker>(ColorProperty, ShowAlphaProperty);
        }

        public ColorPicker()
        {
            Cursor = new Cursor(StandardCursorType.Cross);
            ClipToBounds = true;
        }

        public event EventHandler? ColorChanging;

        public event EventHandler? ColorCommitted;

        public Color Color
        {
            get => GetValue(ColorProperty);
            set => SetValue(ColorProperty, value);
        }

        public bool ShowAlpha
        {
            get => GetValue(ShowAlphaProperty);
            set => SetValue(ShowAlphaProperty, value);
        }

        public bool IsDragging => _dragRegion != DragRegion.None;

        protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
        {
            base.OnPropertyChanged(change);

            if (change.Property == ColorProperty && !_isSettingColorFromInput && !IsDragging)
            {
                SyncFromColor(Color);
            }
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            double width = double.IsNaN(Width) ? 220.0 : Width;
            double height = double.IsNaN(Height) ? 180.0 : Height;

            return new Size(width, height);
        }

        public override void Render(DrawingContext context)
        {
            Rect saturationValueRect = GetSaturationValueRect();
            Rect hueRect = GetHueRect();

            Color pureHue = HsvToRgb(_hue, 1.0, 1.0, 1.0);
            context.FillRectangle(new SolidColorBrush(pureHue), saturationValueRect);
            context.FillRectangle(WhiteToClearBrush, saturationValueRect);
            context.FillRectangle(ClearToBlackBrush, saturationValueRect);
            context.DrawRectangle(null, OutlinePen, saturationValueRect);

            context.FillRectangle(HueBrush, hueRect);
            context.DrawRectangle(null, OutlinePen, hueRect);

            Point saturationValueMarker = new Point(
                saturationValueRect.X + _saturation * saturationValueRect.Width,
                saturationValueRect.Y + (1.0 - _value) * saturationValueRect.Height);

            context.DrawEllipse(null, MarkerOuterPen, saturationValueMarker, 5.0, 5.0);
            context.DrawEllipse(null, MarkerInnerPen, saturationValueMarker, 5.0, 5.0);

            DrawStripMarker(context, hueRect, _hue / 360.0);

            if (ShowAlpha)
            {
                Rect alphaRect = GetAlphaRect();
                Color opaque = HsvToRgb(_hue, _saturation, _value, 1.0);

                using (context.PushClip(alphaRect))
                {
                    DrawCheckerboard(context, alphaRect);
                }

                context.FillRectangle(new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
                    GradientStops =
                    {
                        new GradientStop(opaque, 0.0),
                        new GradientStop(Color.FromArgb(0, opaque.R, opaque.G, opaque.B), 1.0)
                    }
                }, alphaRect);
                context.DrawRectangle(null, OutlinePen, alphaRect);

                DrawStripMarker(context, alphaRect, 1.0 - _alpha);
            }
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            base.OnPointerPressed(e);

            if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            {
                return;
            }

            Point position = e.GetPosition(this);

            _dragRegion = HitTestRegion(position);

            if (_dragRegion == DragRegion.None)
            {
                return;
            }

            e.Pointer.Capture(this);
            e.Handled = true;

            ApplyDrag(position);
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);

            if (IsDragging)
            {
                ApplyDrag(e.GetPosition(this));
                e.Handled = true;
            }
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (IsDragging)
            {
                e.Pointer.Capture(null);
                e.Handled = true;
            }
        }

        protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
        {
            base.OnPointerCaptureLost(e);

            if (IsDragging)
            {
                _dragRegion = DragRegion.None;
                ColorCommitted?.Invoke(this, EventArgs.Empty);
            }
        }

        private void ApplyDrag(Point position)
        {
            switch (_dragRegion)
            {
                case DragRegion.SaturationValue:
                {
                    Rect rect = GetSaturationValueRect();
                    _saturation = Math.Clamp((position.X - rect.X) / rect.Width, 0.0, 1.0);
                    _value = 1.0 - Math.Clamp((position.Y - rect.Y) / rect.Height, 0.0, 1.0);
                    break;
                }
                case DragRegion.Hue:
                {
                    Rect rect = GetHueRect();
                    _hue = Math.Clamp((position.Y - rect.Y) / rect.Height, 0.0, 1.0) * 360.0;
                    break;
                }
                case DragRegion.Alpha:
                {
                    Rect rect = GetAlphaRect();
                    _alpha = 1.0 - Math.Clamp((position.Y - rect.Y) / rect.Height, 0.0, 1.0);
                    break;
                }
                default:
                    return;
            }

            Color color = HsvToRgb(_hue, _saturation, _value, _alpha);

            if (color == Color)
            {
                // The byte color didn't move, but the markers might have.
                InvalidateVisual();
                return;
            }

            _isSettingColorFromInput = true;

            try
            {
                SetCurrentValue(ColorProperty, color);
            }
            finally
            {
                _isSettingColorFromInput = false;
            }

            ColorChanging?.Invoke(this, EventArgs.Empty);
        }

        private void SyncFromColor(Color color)
        {
            _alpha = color.A / 255.0;

            if (HsvToRgb(_hue, _saturation, _value, 1.0) == Color.FromRgb(color.R, color.G, color.B))
            {
                InvalidateVisual();
                return;
            }

            RgbToHsv(color, out double hue, out double saturation, out double value);

            if (saturation > 0.0 && value > 0.0)
            {
                _hue = hue;
            }

            if (value > 0.0)
            {
                _saturation = saturation;
            }

            _value = value;

            InvalidateVisual();
        }

        private DragRegion HitTestRegion(Point position)
        {
            if (GetSaturationValueRect().Inflate(4.0).Contains(position))
            {
                return DragRegion.SaturationValue;
            }

            if (GetHueRect().Inflate(4.0).Contains(position))
            {
                return DragRegion.Hue;
            }

            if (ShowAlpha && GetAlphaRect().Inflate(4.0).Contains(position))
            {
                return DragRegion.Alpha;
            }

            return DragRegion.None;
        }

        private int StripCount => ShowAlpha ? 2 : 1;

        private Rect GetSaturationValueRect()
        {
            double width = Math.Max(0.0, Bounds.Width - StripCount * (StripWidth + StripSpacing));

            return new Rect(0.5, 0.5, Math.Max(0.0, width - 1.0), Math.Max(0.0, Bounds.Height - 1.0));
        }

        private Rect GetHueRect()
        {
            Rect saturationValueRect = GetSaturationValueRect();

            return new Rect(saturationValueRect.Right + StripSpacing, saturationValueRect.Y, StripWidth, saturationValueRect.Height);
        }

        private Rect GetAlphaRect()
        {
            Rect hueRect = GetHueRect();

            return new Rect(hueRect.Right + StripSpacing, hueRect.Y, StripWidth, hueRect.Height);
        }

        private static void DrawStripMarker(DrawingContext context, Rect strip, double position)
        {
            double y = strip.Y + Math.Clamp(position, 0.0, 1.0) * strip.Height;
            Rect marker = new Rect(strip.X - 2.0, y - 2.5, strip.Width + 4.0, 5.0);

            context.DrawRectangle(null, MarkerOuterPen, marker, 2.0, 2.0);
            context.DrawRectangle(null, MarkerInnerPen, marker, 2.0, 2.0);
        }

        private static void DrawCheckerboard(DrawingContext context, Rect rect)
        {
            context.FillRectangle(CheckerLightBrush, rect);

            int columns = (int)Math.Ceiling(rect.Width / CheckerSize);
            int rows = (int)Math.Ceiling(rect.Height / CheckerSize);

            for (int row = 0; row < rows; row++)
            {
                for (int column = row % 2; column < columns; column += 2)
                {
                    context.FillRectangle(CheckerDarkBrush, new Rect(rect.X + column * CheckerSize, rect.Y + row * CheckerSize, CheckerSize, CheckerSize));
                }
            }
        }

        public static Color HsvToRgb(double hue, double saturation, double value, double alpha)
        {
            double sector = (hue % 360.0) / 60.0;
            double chroma = value * saturation;
            double x = chroma * (1.0 - Math.Abs(sector % 2.0 - 1.0));
            double m = value - chroma;

            (double r, double g, double b) = (int)sector switch
            {
                0 => (chroma, x, 0.0),
                1 => (x, chroma, 0.0),
                2 => (0.0, chroma, x),
                3 => (0.0, x, chroma),
                4 => (x, 0.0, chroma),
                _ => (chroma, 0.0, x)
            };

            return Color.FromArgb(ToByte(alpha), ToByte(r + m), ToByte(g + m), ToByte(b + m));
        }

        public static void RgbToHsv(Color color, out double hue, out double saturation, out double value)
        {
            double r = color.R / 255.0;
            double g = color.G / 255.0;
            double b = color.B / 255.0;

            double max = Math.Max(r, Math.Max(g, b));
            double min = Math.Min(r, Math.Min(g, b));
            double delta = max - min;

            value = max;
            saturation = max > 0.0 ? delta / max : 0.0;

            if (delta <= 0.0)
            {
                hue = 0.0;
            }
            else if (max == r)
            {
                hue = 60.0 * (((g - b) / delta + 6.0) % 6.0);
            }
            else if (max == g)
            {
                hue = 60.0 * ((b - r) / delta + 2.0);
            }
            else
            {
                hue = 60.0 * ((r - g) / delta + 4.0);
            }
        }

        private static byte ToByte(double channel)
        {
            return (byte)Math.Round(Math.Clamp(channel, 0.0, 1.0) * 255.0);
        }
    }
}
