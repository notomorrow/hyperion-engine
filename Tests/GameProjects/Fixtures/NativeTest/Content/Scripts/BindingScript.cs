using Hyperion;

// Uses the C++ game's classes through NativeTestGame.Bindings.dll, which CodeGen writes and the game's build compiles
public class BindingScript : Script
{
    [HypField(Editor = true, Serialize = true)]
    public NativeTestPickup? Pickup = null;

    [HypField(Editor = true, Serialize = true)]
    public NativeTestRarity Rarity = NativeTestRarity.Rare;

    public string LastDescription = string.Empty;

    public override void Update(float deltaTime)
    {
        if (Pickup is NativeTestMedkit medkit)
        {
            LastDescription = $"{medkit.PickupName} heals {medkit.HealAmount * medkit.Quantity} ({medkit.GetTotalHealing()})";
        }
        else if (Pickup != null)
        {
            LastDescription = $"{Pickup.PickupName} x{Pickup.Quantity} ({Pickup.GetDoubledQuantity()})";
        }
    }
}
