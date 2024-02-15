#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererInstance.hpp>

namespace hyperion {
namespace renderer {

DescriptorTable *g_static_descriptor_table = new DescriptorTable();

DescriptorDeclaration *DescriptorSetDeclaration::FindDescriptorDeclaration(const String &name) const
{
    for (uint slot_index = 0; slot_index < DESCRIPTOR_SLOT_MAX; slot_index++) {
        for (const DescriptorDeclaration &decl : slots[slot_index]) {
            if (decl.name == name) {
                return const_cast<DescriptorDeclaration *>(&decl);
            }
        }
    }

    return nullptr;
}

uint DescriptorSetDeclaration::CalculateFlatIndex(DescriptorSlot slot, const String &name) const
{
    AssertThrow(slot != DESCRIPTOR_SLOT_NONE && slot < DESCRIPTOR_SLOT_MAX);

    uint flat_index = 0;

    for (uint slot_index = 0; slot_index < uint(slot); slot_index++) {
        if (slot_index == uint(slot) - 1) {
            uint decl_index = 0;

            for (const DescriptorDeclaration &decl : slots[slot_index]) {
                if (decl.name == name) {
                    return flat_index + decl_index;
                }

                decl_index++;
            }
        }

        flat_index += slots[slot_index].Size();
    }

    return uint(-1);
}

DescriptorSetDeclaration *DescriptorTable::FindDescriptorSetDeclaration(Name name) const
{
    for (const DescriptorSetDeclaration &decl : declarations) {
        if (decl.name == name) {
            return const_cast<DescriptorSetDeclaration *>(&decl);
        }
    }

    return nullptr;
}

DescriptorSetDeclaration *DescriptorTable::AddDescriptorSet(DescriptorSetDeclaration descriptor_set)
{
    declarations.PushBack(std::move(descriptor_set));

    return &declarations.Back();
}

static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
        #define HYP_DESCRIPTOR_SETS_DEFINE
        #define HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE g_static_descriptor_table
        #include <rendering/inl/DescriptorSets.inl>
        #undef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
        #undef HYP_DESCRIPTOR_SETS_DEFINE
    }
} g_global_descriptor_sets_declarations;

} // namespace renderer
} // namespace hyperion