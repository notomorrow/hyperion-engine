using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Media;
using Avalonia.Platform;

namespace Hyperion.Editor.Controls
{
    public class Codicon : Control
    {
        public static readonly StyledProperty<string> KindProperty =
            AvaloniaProperty.Register<Codicon, string>(nameof(Kind), string.Empty);

        public static readonly StyledProperty<double> SizeProperty =
            AvaloniaProperty.Register<Codicon, double>(nameof(Size), 16.0);

        public static readonly StyledProperty<IBrush?> ForegroundProperty =
            TextElement.ForegroundProperty.AddOwner<Codicon>();

        static Codicon()
        {
            AffectsRender<Codicon>(KindProperty, SizeProperty, ForegroundProperty);
            AffectsMeasure<Codicon>(KindProperty, SizeProperty);
        }

        public string Kind
        {
            get => GetValue(KindProperty);
            set => SetValue(KindProperty, value);
        }

        public double Size
        {
            get => GetValue(SizeProperty);
            set => SetValue(SizeProperty, value);
        }

        public IBrush? Foreground
        {
            get => GetValue(ForegroundProperty);
            set => SetValue(ForegroundProperty, value);
        }

        private static readonly Dictionary<string, StreamGeometry?> GeometryCache = new();

        protected override Size MeasureOverride(Size availableSize)
        {
            double width = double.IsNaN(Width) ? Size : Width;
            double height = double.IsNaN(Height) ? Size : Height;
            return new Size(width, height);
        }

        public override void Render(DrawingContext context)
        {
            string kind = Kind;
            if (string.IsNullOrEmpty(kind))
            {
                return;
            }

            StreamGeometry? geometry = ResolveGeometry(kind);
            if (geometry is null)
            {
                return;
            }

            Rect sourceBounds = geometry.Bounds;
            if (sourceBounds.Width <= 0 || sourceBounds.Height <= 0)
            {
                return;
            }

            double targetWidth = double.IsNaN(Width) ? Size : Width;
            double targetHeight = double.IsNaN(Height) ? Size : Height;

            double scale = Math.Min(targetWidth / sourceBounds.Width, targetHeight / sourceBounds.Height);
            if (!double.IsFinite(scale) || scale <= 0)
            {
                return;
            }

            double offsetX = ((Bounds.Width - sourceBounds.Width * scale) * 0.5) - (sourceBounds.X * scale);
            double offsetY = ((Bounds.Height - sourceBounds.Height * scale) * 0.5) - (sourceBounds.Y * scale);

            using (context.PushTransform(Matrix.CreateScale(scale, scale) * Matrix.CreateTranslation(offsetX, offsetY)))
            {
                context.DrawGeometry(Foreground, null, geometry);
            }
        }

        private static StreamGeometry? ResolveGeometry(string kind)
        {
            lock (GeometryCache)
            {
                string fileName = NormalizeKind(kind);
                if (GeometryCache.TryGetValue(fileName, out StreamGeometry? cached))
                {
                    return cached;
                }

                StreamGeometry? geometry = LoadGeometry(fileName);
                GeometryCache[fileName] = geometry;
                return geometry;
            }
        }

        private static string NormalizeKind(string kind)
        {
            if (kind.Contains('-'))
            {
                return kind.ToLowerInvariant();
            }

            StringBuilder sb = new(kind.Length + 8);
            for (int i = 0; i < kind.Length; i++)
            {
                char c = kind[i];
                if (char.IsUpper(c) && i > 0)
                {
                    sb.Append('-');
                }

                sb.Append(char.ToLowerInvariant(c));
            }

            return sb.ToString();
        }

        private static StreamGeometry? LoadGeometry(string fileName)
        {
            try
            {
                Uri uri = new($"avares://Hyperion.Editor/Assets/Icons/{fileName}.svg");
                if (!AssetLoader.Exists(uri))
                {
                    return null;
                }

                using Stream stream = AssetLoader.Open(uri);
                XDocument document = XDocument.Load(stream);
                XNamespace ns = document.Root?.Name.Namespace ?? XNamespace.None;

                IEnumerable<string> parts = document
                    .Descendants(ns + "path")
                    .Attributes("d")
                    .Select(a => a.Value)
                    .Where(d => !string.IsNullOrWhiteSpace(d));

                string combined = string.Join(" ", parts);
                if (combined.Length == 0)
                {
                    return null;
                }

                return StreamGeometry.Parse(combined);
            }
            catch
            {
                return null;
            }
        }
    }
}
