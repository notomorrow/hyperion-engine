#ifndef AEX_GENERATOR_HPP
#define AEX_GENERATOR_HPP

#include <script/compiler/emit/BuildableVisitor.hpp>
#include <script/compiler/emit/codegen/InternalByteStream.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/debug/Debug.hpp>

#include <map>
#include <sstream>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

class CodeGenerator : public BuildableVisitor
{
public:
    CodeGenerator(BuildParams &buildParams);
    virtual ~CodeGenerator() = default;

    InternalByteStream &GetInternalByteStream() { return m_ibs; }
    const InternalByteStream &GetInternalByteStream() const { return m_ibs; }

    void Bake();

    virtual void Visit(BytecodeChunk *);
    virtual void Visit(LabelMarker *);
    virtual void Visit(Jump *);
    virtual void Visit(Comparison *);
    virtual void Visit(FunctionCall *);
    virtual void Visit(Return *);
    virtual void Visit(StoreLocal *);
    virtual void Visit(PopLocal *);
    virtual void Visit(LoadRef *);
    virtual void Visit(LoadDeref *);
    virtual void Visit(ConstI32 *);
    virtual void Visit(ConstI64 *);
    virtual void Visit(ConstU32 *);
    virtual void Visit(ConstU64 *);
    virtual void Visit(ConstF32 *);
    virtual void Visit(ConstF64 *);
    virtual void Visit(ConstBool *);
    virtual void Visit(ConstNull *);
    virtual void Visit(BuildableTryCatch *);
    virtual void Visit(BuildableFunction *);
    virtual void Visit(BuildableType *);
    virtual void Visit(BuildableString *);
    virtual void Visit(StorageOperation *);
    virtual void Visit(Comment *);
    virtual void Visit(SymbolExport *);
    virtual void Visit(CastOperation *);
    virtual void Visit(RawOperation<> *);

private:
    BuildParams &buildParams;
    InternalByteStream m_ibs;
};

} // namespace hyperion::compiler

#endif
