using System;
using System.Collections.Generic;
using System.Text;
using Hyperion;

namespace Hyperion.Editor.UI.Overlays;

public class FpsCounter : EditorDebugOverlayBase
{
    private static readonly List<KeyValuePair<int, Color>> fpsColors = new List<KeyValuePair<int, Color>>
    {
        new KeyValuePair<int, Color>(30, new Color(1.0f, 0.0f, 0.0f, 1.0f)),
        new KeyValuePair<int, Color>(60, new Color(1.0f, 1.0f, 0.0f, 1.0f)),
        new KeyValuePair<int, Color>(int.MaxValue, new Color(0.0f, 1.0f, 0.0f, 1.0f))
    };

    private UIText? memoryUsageTextElement;
    private UIText? fpsTextElement;
    private UIText? countersTextElement;
    private float deltaAccumGame = 0.0f;
    private int numTicksGame = 0;
    private World world;

    public FpsCounter(World world)
    {
        this.world = world;
    }

    public override UIObject CreateUIObject(UIObject spawnParent)
    {
        UIListView panel = spawnParent.Spawn<UIListView>(new Name("FpsCounter_Panel"), new Vec2i(0, 0), new UIObjectSize(100, UIObjectSize.Percent, 0, UIObjectSize.Auto));
        panel.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));

        UIListView renderListView = spawnParent.Spawn<UIListView>(new Name("FpsCounter_RenderListView"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
        renderListView.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));
        renderListView.SetOrientation(UIListViewOrientation.Horizontal);
        renderListView.SetTextSize(8);

        UIText fpsTextElement = renderListView.Spawn<UIText>(new Name("FpsCounter_Fps"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
        fpsTextElement.SetText("0 fps, 0.00 ms/frame (avg: 0.00, min: 0.00, max: 0.00)");
        fpsTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        fpsTextElement.SetPadding(new Vec2i(5, 5));
        renderListView.AddChildUIObject(fpsTextElement);
        this.fpsTextElement = fpsTextElement;

        UIText countersTextElement = renderListView.Spawn<UIText>(new Name("FpsCounter_Counters"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
        countersTextElement.SetText("draw calls: 0, Tris: 0");
        countersTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        countersTextElement.SetPadding(new Vec2i(5, 5));
        renderListView.AddChildUIObject(countersTextElement);
        this.countersTextElement = countersTextElement;

        UIText memoryUsageTextElement = renderListView.Spawn<UIText>(new Name("FpsCounter_MemoryUsage"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
        memoryUsageTextElement.SetText(".NET Memory Usage: 0 MB");
        memoryUsageTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        memoryUsageTextElement.SetPadding(new Vec2i(5, 5));
        renderListView.AddChildUIObject(memoryUsageTextElement);
        this.memoryUsageTextElement = memoryUsageTextElement;

        panel.AddChildUIObject(renderListView);

        return panel;
    }

    public override int GetPlacement()
    {
        return 1; // Bottom-left corner
    }

    public override Name GetName()
    {
        return new Name("FpsCounter");
    }

    public override bool IsEnabled()
    {
        return true;
    }

    public override void Update(float delta)
    {
        deltaAccumGame += delta;
        numTicksGame++;

        if (deltaAccumGame >= 1.0f)
        {
            if (memoryUsageTextElement != null)
            {
                long memoryUsage = GC.GetTotalMemory(false) / 1024 / 1024;

                ((UIText)memoryUsageTextElement).SetText(string.Format(".NET Memory Usage: {0} MB", memoryUsage));
            }

            deltaAccumGame = 0.0f;
            numTicksGame = 0;
        }

        var renderStats = world.GetRenderStats();

        if (fpsTextElement != null)
        {
            ((UIText)fpsTextElement).SetText(string.Format("{0} fps, {1:0.00} ms/frame (avg: {2:0.00}, min: {3:0.00}, max: {4:0.00})",
                (int)renderStats.framesPerSecond, renderStats.millisecondsPerFrame,
                renderStats.millisecondsPerFrameAvg, renderStats.millisecondsPerFrameMin, renderStats.millisecondsPerFrameMax));
            fpsTextElement.SetTextColor(GetFpsColor((int)renderStats.framesPerSecond));
        }

        if (countersTextElement != null)
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendFormat("DrawCalls: {0}", renderStats.counts[RenderStatsCountType.DrawCalls]);

            if (renderStats.counts[RenderStatsCountType.InstancedDrawCalls] > 0)
                sb.AppendFormat(", Instanced: {0}", renderStats.counts[RenderStatsCountType.InstancedDrawCalls]);

            if (renderStats.counts[RenderStatsCountType.DebugDraws] > 0)
                sb.AppendFormat(", DebugDraw: {0}", renderStats.counts[RenderStatsCountType.DebugDraws]);

            sb.AppendFormat(", Tris: {0}", renderStats.counts[RenderStatsCountType.Triangles]);
            sb.AppendFormat(", Groups: {0}", renderStats.counts[RenderStatsCountType.RenderGroups]);
            sb.AppendFormat(", Views: {0}", renderStats.counts[RenderStatsCountType.Views]);
            sb.AppendFormat(", Textures: {0}", renderStats.counts[RenderStatsCountType.Textures]);
            sb.AppendFormat(", Materials: {0}", renderStats.counts[RenderStatsCountType.Materials]);

            if (renderStats.counts[RenderStatsCountType.Lights] > 0)
                sb.AppendFormat(", Lights: {0}", renderStats.counts[RenderStatsCountType.Lights]);

            if (renderStats.counts[RenderStatsCountType.LightmapVolumes] > 0)
                sb.AppendFormat(", LightmapVolumes: {0}", renderStats.counts[RenderStatsCountType.LightmapVolumes]);

            if (renderStats.counts[RenderStatsCountType.EnvProbes] > 0)
                sb.AppendFormat(", EnvProbes: {0}", renderStats.counts[RenderStatsCountType.EnvProbes]);

            ((UIText)countersTextElement).SetText(sb.ToString());
        }
    }

    private static Color GetFpsColor(int fps)
    {
        for (int i = 0; i < fpsColors.Count; i++)
        {
            if (fps <= fpsColors[i].Key)
            {
                return fpsColors[i].Value;
            }
        }

        return fpsColors[fpsColors.Count - 1].Value;
    }
}