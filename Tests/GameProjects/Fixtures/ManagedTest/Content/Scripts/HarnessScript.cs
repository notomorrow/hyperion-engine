using Hyperion;

public class HarnessScript : Script
{
    [HypField(Editor = true, Serialize = true)]
    public TestPickup? Pickup = null;

    [HypField(Editor = true, Serialize = true)]
    public int UpdateCount = 0;

    public string LastDescription = string.Empty;

    public override void Update(float deltaTime)
    {
        UpdateCount++;

        LastDescription = Pickup != null
            ? $"{Pickup.PickupName} x{Pickup.Quantity}"
            : "nothing";
    }
}
