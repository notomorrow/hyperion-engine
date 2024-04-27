/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARG_PARSE_HPP
#define HYPERION_ARG_PARSE_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace sys {

using CommandLineArgumentValue = Variant<String, int, float, bool>;

enum CommandLineArgumentType
{
    CLAT_STRING = 1,
    CLAT_INT,
    CLAT_FLOAT,
    CLAT_BOOL,
    CLAT_ENUM
};

class ArgParse;

class CommandLineArguments
{
    String                                          command;
    Array<Pair<String, CommandLineArgumentValue>>   values;

public:
    friend class ArgParse;

    using Iterator = Array<Pair<String, CommandLineArgumentValue>>::Iterator;
    using ConstIterator = Array<Pair<String, CommandLineArgumentValue>>::ConstIterator;

    CommandLineArgumentValue operator[](const String &key) const
    {
        auto it = Find(key);

        if (it == values.End()) {
            return CommandLineArgumentValue { };
        }

        return it->second;
    }

    const String &GetCommand() const
        { return command; }

    const Array<Pair<String, CommandLineArgumentValue>> &GetValues() const
        { return values; }

    SizeType Size() const
        { return values.Size(); }

    ConstIterator Find(const String &key) const
        { return values.FindIf([key](const auto &pair) { return pair.first == key; }); }

    bool Contains(const String &key) const
        { return Find(key) != values.End(); }

    HYP_DEF_STL_BEGIN_END(
        values.Begin(),
        values.End()
    )
};

class ArgParse
{
public:
    struct ParseResult
    {
        CommandLineArguments    result;
        bool                    ok;
        Optional<String>        message;

        explicit operator bool() const
            { return ok; }
    };

    using ArgFlags = uint32;

    enum ArgFlagBits : ArgFlags
    {
        ARG_FLAGS_NONE      = 0x0,
        ARG_FLAGS_REQUIRED  = 0x1
    };

    struct ArgumentDefinition
    {
        String                      name;
        Optional<String>            shorthand;
        ArgFlags                    flags;
        CommandLineArgumentType     type;
        CommandLineArgumentValue    default_value;
        Optional<Array<String>>     enum_values;
    };

    // Add an argument - may be a string, int, float, bool.
    HYP_API void Add(
        String name,
        String shorthand = String::empty,
        ArgFlags flags = ARG_FLAGS_NONE,
        CommandLineArgumentType type = CLAT_STRING,
        CommandLineArgumentValue default_value = { }
    );

    // Add an enum argument
    HYP_API void Add(
        String name,
        String shorthand = String::empty,
        ArgFlags flags = ARG_FLAGS_NONE,
        Optional<Array<String>> enum_values = { },
        CommandLineArgumentValue = { }
    );

    HYP_API ParseResult Parse(int argc, char **argv) const;
    HYP_API ParseResult Parse(const String &command, const Array<String> &args) const;

private:
    Array<ArgumentDefinition>   m_definitions;
};

} // namespace sys

using sys::ArgParse;

using sys::CommandLineArguments;
using sys::CommandLineArgumentValue;
using sys::CommandLineArgumentType;

} // namespace hyperion

#endif