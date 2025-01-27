
using System;
using System.IO;
using Hyperion;

namespace Hyperion
{
    namespace Editor
    {
        public class ContentBrowser : UIEventHandler
        {
            public override void Init(Entity entity)
            {
                base.Init(entity);
            }

            [UIEvent(AllowNested = true)]
            public void ImportClicked()
            {
                Logger.Log(LogType.Info, "Import content clicked");
            }
        }
    }
}