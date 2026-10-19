namespace Hyperion
{
    public enum TargetPlatform : uint
    {
        Windows = 0,
        Mac,
        Linux,
        Android,
        IOS,
        Web
    }

    [ClassBinding(Name = "PackagingPlatformOverride")]
    public class PackagingPlatformOverride : ObjectBase
    {
    }

    [ClassBinding(Name = "PackagingSettings")]
    public class PackagingSettings : ObjectBase
    {
    }
}
