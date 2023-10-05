#ifndef INSTRUCTION_STREAM_HPP
#define INSTRUCTION_STREAM_HPP

#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>

#include <core/lib/Optional.hpp>

#include <Types.hpp>

#include "script/compiler/Tree.hpp"

namespace hyperion::compiler {

enum InstructionStreamContextType : UInt32
{
    INSTRUCTION_STREAM_CONTEXT_DEFAULT = 0,
    INSTRUCTION_STREAM_CONTEXT_LOOP
};

class InstructionStreamContext
{
public:
    InstructionStreamContext(
        InstructionStreamContext *parent = nullptr,
        InstructionStreamContextType type = INSTRUCTION_STREAM_CONTEXT_DEFAULT
    ) : m_parent(parent),
        m_type(type),
        m_next_label_id(0)
    {
    }

    InstructionStreamContext(const InstructionStreamContext &other) = delete;
    InstructionStreamContext &operator=(const InstructionStreamContext &other) = delete;
    InstructionStreamContext(InstructionStreamContext &&other) noexcept = delete;
    InstructionStreamContext &operator=(InstructionStreamContext &&other) noexcept = delete;
    ~InstructionStreamContext() = default;

    InstructionStreamContextType GetType() const
        { return m_type; }

    LabelId NewLabel()
    {
        const LabelId label_id = NextLabelID();
        m_labels.PushBack(LabelInfo { label_id, LabelPosition(-1), HYP_NAME(LabelNotNamed) });
        return label_id;
    }

    LabelId NewLabel(Name name)
    {
        AssertThrowMsg(!FindLabelByName(name).HasValue(), "Cannot duplicate label identifier");

        const LabelId label_id = NextLabelID();
        m_labels.PushBack(LabelInfo { label_id, LabelPosition(-1), name });
        return label_id;
    }

    Optional<LabelId> FindLabelByName(Name name) const
    {
        const auto it = m_labels.FindIf([name](const LabelInfo &label_info) {
            return label_info.name == name;
        });

        if (it == m_labels.End()) {
            return Optional<LabelId> { };
        }

        return it->label_id;
    }

private:
    LabelId NextLabelID()
    {
        if (m_parent) {
            return m_parent->NextLabelID();
        }

        return m_next_label_id++;
    }

    InstructionStreamContext        *m_parent;
    InstructionStreamContextType    m_type;
    Array<LabelInfo>                m_labels;

    UInt32                          m_next_label_id;
};

struct InstructionStreamContextGuard : TreeNodeGuard<InstructionStreamContext>
{
    InstructionStreamContextGuard(
        Tree<InstructionStreamContext> *tree,
        InstructionStreamContextType type = INSTRUCTION_STREAM_CONTEXT_DEFAULT
    ) : TreeNodeGuard(tree, &tree->Root(), type)
    {
    }
};

class InstructionStream
{
public:
    InstructionStream();
    InstructionStream(const InstructionStream &other);

    UInt8 GetCurrentRegister() const { return m_register_counter; }
    UInt8 IncRegisterUsage() { return ++m_register_counter; }
    UInt8 DecRegisterUsage() { return --m_register_counter; }

    Int GetStackSize() const
        { return m_stack_size; }

    void SetStackSize(Int stack_size)
        { m_stack_size = stack_size; }

    Int IncStackSize()
        { return ++m_stack_size; }

    Int DecStackSize()
    {
        AssertThrowMsg(m_stack_size > 0, "Compiler stack size record invalid");

        return --m_stack_size;
    }

    Int NewStaticId() { return m_static_id++; }

    void AddStaticObject(const StaticObject &static_object)
        { m_static_objects.PushBack(static_object); }

    Int FindStaticObject(const StaticObject &static_object) const;

    Tree<InstructionStreamContext> &GetContextTree()
        { return m_context_tree; }

    const Tree<InstructionStreamContext> &GetContextTree() const
        { return m_context_tree; }

private:
    // incremented and decremented each time a register
    // is used/unused
    UInt8                               m_register_counter;
    // incremented each time a variable is pushed,
    // decremented each time a stack frame is closed
    Int                                 m_stack_size;
    // the current static object id
    Int                                 m_static_id;

    Array<StaticObject>                 m_static_objects;

    Tree<InstructionStreamContext>      m_context_tree;
};

} // namespace hyperion::compiler

#endif
