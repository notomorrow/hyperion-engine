
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    public class ContentBrowser : UIEventHandler
    {
        public override void Init(Entity entity)
        {
            base.Init(entity);
        }

        [UIEvent(AllowNested = true)]
        public void ImportContentClicked()
        {
            Logger.Log(LogType.Info, "Import content clicked");
        }
    }
}