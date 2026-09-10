using System;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion.Editor
{
    /// <summary>
    /// Editor-facing API for per-swatch property overrides on entities
    /// (the <c>$SwatchOverrides</c> HMF schema section).
    /// Non-value operations route through <see cref="EditorSubsystem"/> reflection methods;
    /// BoxedValue-carrying operations use dedicated P/Invoke exports (BoxedValue is not
    /// supported by the code generator's parameter/return mapping).
    /// All methods must be called on the sim thread.
    /// </summary>
    public static class EntitySwatchOverrides
    {
        private static EditorSubsystem? Subsystem => EngineManager.EditorGame?.EditorSubsystem;

        public static Name[] GetSetSwatchNames(Entity entity)
        {
            // Note: the generated wrapper for Array<Name> returns a non-generic Array of boxed Names.
            Array sets = Subsystem?.GetEntitySwatchOverrideSets(entity);

            if (sets == null)
            {
                return Array.Empty<Name>();
            }

            Name[] result = new Name[sets.Length];

            for (int i = 0; i < sets.Length; i++)
            {
                result[i] = sets.GetValue(i) is Name name ? name : Name.Invalid;
            }

            return result;
        }

        public static bool HasSet(Entity entity, Name swatchName)
        {
            return Subsystem?.EntityHasSwatchOverrideSet(entity, swatchName) ?? false;
        }

        /// <summary>True when the entity's override set for the swatch contains at least one entry.</summary>
        public static bool HasValues(Entity entity, Name swatchName)
        {
            return Subsystem?.EntityHasSwatchOverrideValues(entity, swatchName) ?? false;
        }

        public static void AddSet(Entity entity, Name swatchName)
        {
            Subsystem?.EntityAddSwatchOverrideSet(entity, swatchName);
        }

        public static bool RemoveSet(Entity entity, Name swatchName)
        {
            return Subsystem?.EntityRemoveSwatchOverrideSet(entity, swatchName) ?? false;
        }

        public static bool IsPropertyOverridden(Entity entity, Name swatchName, Name propertyName)
        {
            return Subsystem?.IsEntityPropertyOverridden(entity, swatchName, propertyName) ?? false;
        }

        public static bool RemoveValue(Entity entity, Name swatchName, Name propertyName)
        {
            return Subsystem?.EntityRemoveSwatchOverrideValue(entity, swatchName, propertyName) ?? false;
        }

        public static Name GetAppliedSwatch(Entity entity)
        {
            return Subsystem?.GetEntityAppliedOverrideSwatch(entity) ?? Name.Invalid;
        }

        public static void Apply(Entity entity, Name swatchName)
        {
            Subsystem?.EntityApplySwatchOverrides(entity, swatchName);
        }

        public static void Revert(Entity entity)
        {
            Subsystem?.EntityRevertSwatchOverrides(entity);
        }

        /// <summary>
        /// Global editor toggle: when enabled, editor edits apply to the active swatch's override
        /// set only; when disabled, edits apply to base (keeping existing active-swatch overrides
        /// in sync).
        /// </summary>
        public static void SetOverrideMode(bool enabled)
        {
            Subsystem?.SetSwatchOverrideMode(enabled);
        }

        public static bool SetValue(Entity entity, Name swatchName, Name propertyName, BoxedValue value)
        {
            return EntitySwatchOverrides_SetValue(entity.NativeAddress, swatchName.HashCode, propertyName.HashCode, ref value.Buffer);
        }

        /// <summary>
        /// Writes a property's base value (through its setter), keeping the applied swatch's
        /// base snapshot in sync so a revert returns to the edited base.
        /// </summary>
        public static bool SetBaseValue(Entity entity, Name propertyName, BoxedValue value)
        {
            return EntitySwatchOverrides_SetBaseValue(entity.NativeAddress, propertyName.HashCode, ref value.Buffer);
        }

        public static bool GetBaseValue(Entity entity, Name swatchName, Name propertyName, out BoxedValue value)
        {
            BoxedValueInternal internalValue = new BoxedValueInternal();

            try
            {
                if (!EntitySwatchOverrides_GetBaseValue(entity.NativeAddress, swatchName.HashCode, propertyName.HashCode, ref internalValue))
                {
                    value = new BoxedValue(null);
                    return false;
                }

                value = BoxedValue.FromBuffer(internalValue);
                return true;
            }
            catch
            {
                internalValue.Dispose();
                throw;
            }
        }

        public static bool GetValue(Entity entity, Name swatchName, Name propertyName, out BoxedValue value)
        {
            BoxedValueInternal internalValue = new BoxedValueInternal();

            try
            {
                if (!EntitySwatchOverrides_GetValue(entity.NativeAddress, swatchName.HashCode, propertyName.HashCode, ref internalValue))
                {
                    value = new BoxedValue(null);
                    return false;
                }

                value = BoxedValue.FromBuffer(internalValue);
                return true;
            }
            catch
            {
                internalValue.Dispose();
                throw;
            }
        }

        [DllImport("hyperion", EntryPoint = "EntitySwatchOverrides_SetValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntitySwatchOverrides_SetValue(IntPtr pEntity, ulong swatchHash, ulong propertyHash, [In] ref BoxedValueInternal pValue);

        [DllImport("hyperion", EntryPoint = "EntitySwatchOverrides_SetBaseValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntitySwatchOverrides_SetBaseValue(IntPtr pEntity, ulong propertyHash, [In] ref BoxedValueInternal pValue);

        [DllImport("hyperion", EntryPoint = "EntitySwatchOverrides_GetBaseValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntitySwatchOverrides_GetBaseValue(IntPtr pEntity, ulong swatchHash, ulong propertyHash, [In, Out] ref BoxedValueInternal pOutValue);

        [DllImport("hyperion", EntryPoint = "EntitySwatchOverrides_GetValue")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool EntitySwatchOverrides_GetValue(IntPtr pEntity, ulong swatchHash, ulong propertyHash, [In, Out] ref BoxedValueInternal pOutValue);
    }
}
