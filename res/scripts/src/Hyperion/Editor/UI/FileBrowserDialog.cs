
using System;
using System.IO;
using System.Collections.Generic;
using Hyperion;

namespace Hyperion
{
    namespace Editor
    {
        [HypClassBinding(IsDynamic = true)]
        public struct FileInfo
        {
            public string Name { get; set; }
            public string Path { get; set; }
            public bool IsDirectory { get; set; }
        }

        public class FileInfoUIElementFactory : UIElementFactoryBase
        {
            public override UIObject CreateUIObject(UIObject parent, object value, object context)
            {
                FileInfo fileInfo = (FileInfo)value;

                UIText text = parent.Spawn<UIText>();
                text.SetText(fileInfo.Name.ToString());
                
                return text;
            }

            public override void UpdateUIObject(UIObject uiObject, object value, object context)
            {
                FileInfo fileInfo = (FileInfo)value;

                UIText text = (UIText)uiObject;
                text.SetText(fileInfo.Name.ToString());
            }
        }

        public class FileBrowserDialog : UIEventHandler
        {
            private string currentPath = "";
            private UIGrid? contentsGrid;
            private UIPanel? noDataPanel;
            private UIButton? backButton;
            private Stack<string> pathStack = new Stack<string>();
            private string filter = "*.obj";

            public override void Init(Entity entity)
            {
                base.Init(entity);

                Logger.Log(LogType.Info, "FileBrowserDialog initialized");
            }

            [UIEvent(AllowNested = true)]
            public override UIEventHandlerResult OnAttached()
            {
                Logger.Log(LogType.Info, "OnAttached");

                HypClass hypClass = HypClass.GetClass<FileInfo>();

                contentsGrid = UIObject.Find<UIGrid>(new Name("File_Browser_Dialog_Contents", weak: true));
                contentsGrid?.SetIsVisible(false);

                UIDataSource dataSource = new UIDataSource(TypeID.ForType<FileInfo>(), new FileInfoUIElementFactory());
                contentsGrid?.SetDataSource(dataSource);

                noDataPanel = UIObject.Find<UIPanel>(new Name("File_Browser_Dialog_NoData_Panel", weak: true));
                noDataPanel?.SetIsVisible(true);

                backButton = UIObject.Find<UIButton>(new Name("File_Browser_Dialog_Back_Button", weak: true));
                backButton?.SetIsVisible(false);

                PushDirectory(Directory.GetCurrentDirectory());

                return UIEventHandlerResult.Ok;
            }

            [UIEvent]
            public UIEventHandlerResult BackButtonClicked()
            {
                Logger.Log(LogType.Info, "Back button clicked");

                PopDirectory();

                return UIEventHandlerResult.Ok;
            }

            private void PushDirectory(string path)
            {
                pathStack.Push(currentPath);
                currentPath = path;
                UpdateContents();

                backButton?.SetIsVisible(pathStack.Count > 1);
            }

            private void PopDirectory()
            {
                if (pathStack.Count > 0)
                {
                    currentPath = pathStack.Pop();
                    UpdateContents();
                }

                backButton?.SetIsVisible(pathStack.Count > 1);
            }

            private void UpdateContents()
            {
                List<FileInfo> files = GetFiles(currentPath);

                UIDataSourceBase? dataSource = contentsGrid?.GetDataSource();
                Assert.Throw(dataSource != null, "DataSource is null");

                dataSource.Clear();

                foreach (FileInfo fileInfo in files)
                {
                    dataSource.Push(new UUID(), fileInfo);
                }

                if (dataSource.Size() == 0)
                {
                    contentsGrid?.SetIsVisible(false);
                    noDataPanel?.SetIsVisible(true);
                }
                else
                {
                    contentsGrid?.SetIsVisible(true);
                    noDataPanel?.SetIsVisible(false);
                }
            }

            private List<FileInfo> GetFiles(string path)
            {
                List<FileInfo> files = new List<FileInfo>();

                try
                {
                    foreach (string directory in Directory.GetDirectories(path))
                    {
                        files.Add(new FileInfo
                        {
                            Name = Path.GetFileName(directory),
                            Path = directory,
                            IsDirectory = true
                        });
                    }

                    // Files
                    string[] filters = filter.Split(';', StringSplitOptions.RemoveEmptyEntries);
                    foreach (string file in Directory.GetFiles(path))
                    {
                        string fileExtension = Path.GetExtension(file).ToLowerInvariant();
                        bool matchesFilter = false;

                        foreach (string f in filters)
                        {
                            string filterExtension = Path.GetExtension(f).ToLowerInvariant();
                            if (filterExtension.Equals(fileExtension, StringComparison.OrdinalIgnoreCase))
                            {
                                matchesFilter = true;
                                break;
                            }
                        }

                        if (!matchesFilter)
                        {
                            continue;
                        }

                        files.Add(new FileInfo
                        {
                            Name = Path.GetFileName(file),
                            Path = file,
                            IsDirectory = false
                        });
                    }
                }
                catch (Exception e)
                {
                    Logger.Log(LogType.Error, $"Error getting files: {e.Message}");
                }

                return files;
            }
        }
    }
}