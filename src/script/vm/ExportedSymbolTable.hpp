#pragma once

#include <script/vm/Value.hpp>
#include <script/Hasher.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/debug/Debug.hpp>

#include <utility>

namespace hyperion {
namespace vm {

class ExportedSymbolTable
{
    using SymbolMap = FlatMap<HashFNV1, Value>;

public:
    ExportedSymbolTable();
    ExportedSymbolTable(const ExportedSymbolTable& other) = delete;
    ~ExportedSymbolTable();

    void MarkAll();

    bool Find(const char* name, Value* out);
    bool Find(HashFNV1 hash, Value* out);
    typename SymbolMap::InsertResult Store(const char* name, const Value& value);
    typename SymbolMap::InsertResult Store(HashFNV1 hash, const Value& value);

private:
    SymbolMap m_symbols;
};

} // namespace vm
} // namespace hyperion

