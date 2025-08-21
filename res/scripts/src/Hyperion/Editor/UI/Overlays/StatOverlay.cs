using System;
using System.Collections.Generic;
using System.Text;
using Hyperion;

namespace Hyperion.Editor.UI.Overlays;

public class StatOverlay : EditorDebugOverlayBase
{
    public StatOverlay()
    {
    }

    public override UIObject CreateUIObject(UIObject spawnParent)
    {
        // Create a panel for the stats overlay
        UIListView panel = spawnParent.Spawn<UIListView>(new Name("StatOverlay_Panel"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
        panel.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.5f));
        panel.SetTextSize(9);
        panel.SetPadding(new Vec2i(10, 10));

        return panel;
    }

    public override Name GetName()
    {
        return new Name("StatOverlay");
    }

    public override bool IsEnabled()
    {
        return true;
    }

    public override void Update(float delta)
    {
        // @TODO - update stats items for each stat group
    }
}