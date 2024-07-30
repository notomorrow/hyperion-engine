/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARG_PARSE_HPP
#define HYPERION_ARG_PARSE_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/StringView.hpp>

#include <core/Defines.hpp>

#include <util/json/JSON.hpp>

namespace hyperion {

enum class ArgFlags : uint32
{
    NONE        = 0x0,
    REQUIRED    = 0x1
};

HYP_MAKE_ENUM_FLAGS(ArgFlags)

enum class ArgParseFlags : uint32
{
    NONE                = 0x0,
    ALLOW_UNKNOWN_ARGS  = 0x1
};

HYP_MAKE_ENUM_FLAGS(ArgParseFlags)

enum class CommandLineArgumentType
{
    STRING = 1,
    INTEGER,
    FLOAT,
    BOOLEAN,
    ENUM
};

namespace sys {

using CommandLineArgumentValue = json::JSONValue;

struct CommandLineArgumentError
{
    String  message;
};

class ArgParse;

class HYP_API CommandLineArguments
{
    String                                          command;
    Array<Pair<String, CommandLineArgumentValue>>   values;

public:
    friend class ArgParse;

    using Iterator = Array<Pair<String, CommandLineArgumentValue>>::Iterator;
    using ConstIterator = Array<Pair<String, CommandLineArgumentValue>>::ConstIterator;

    const CommandLineArgumentValue &operator[](UTF8StringView key) const;

    HYP_FORCE_INLINE const String &GetCommand() const
        { return command; }

    HYP_FORCE_INLINE const Array<Pair<String, CommandLineArgumentValue>> &GetValues() const
        { return values; }

    HYP_FORCE_INLINE SizeType Size() const
        { return values.Size(); }

    HYP_FORCE_INLINE Iterator Find(UTF8StringView key)
        { return values.FindIf([key](auto &pair) { return pair.first == key; }); }

    HYP_FORCE_INLINE ConstIterator Find(UTF8StringView key) const
        { return values.FindIf([key](const auto &pair) { return pair.first == key; }); }

    HYP_FORCE_INLINE bool Contains(UTF8StringView key) const
        { return Find(key) != values.End(); }

    HYP_NODISCARD static CommandLineArguments Merge(const CommandLineArguments &a, const CommandLineArguments &b);

    HYP_DEF_STL_BEGIN_END(
        values.Begin(),
        values.End()
    )
};

struct ArgParseDefinition
{
    String                              name;
    Optional<String>                    shorthand;
    ArgFlags                            flags;
    CommandLineArgumentType             type;
    Optional<CommandLineArgumentValue>  default_value;
    Optional<Array<String>>             enum_values;
};

struct ArgParseDefinitions
{
    using Iterator = typename Array<ArgParseDefinition>::Iterator;
    using ConstIterator = typename Array<ArgParseDefinition>::ConstIterator;

    Array<ArgParseDefinition>   definitions;

    ArgParseDefinitions()                                                   = default;
    ArgParseDefinitions(const Array<ArgParseDefinition> &definitions)
        : definitions(definitions)
    {
    }

    ArgParseDefinitions(const ArgParseDefinitions &other)                   = default;
    ArgParseDefinitions &operator=(const ArgParseDefinitions &other)        = default;

    ArgParseDefinitions(ArgParseDefinitions &&other) noexcept               = default;
    ArgParseDefinitions &operator=(ArgParseDefinitions &&other) noexcept    = default;

    ~ArgParseDefinitions()                                                  = default;

    // Add an argument - may be a string, int, float, bool.
    HYP_API ArgParseDefinitions &Add(
        const String &name,
        const String &shorthand = String::empty,
        EnumFlags<ArgFlags> flags = ArgFlags::NONE,
        CommandLineArgumentType type = CommandLineArgumentType::STRING,
        const CommandLineArgumentValue &default_value = { }
    );

    // Add an enum argument
    HYP_API ArgParseDefinitions &Add(
        const String &name,
        const String &shorthand = String::empty,
        EnumFlags<ArgFlags> flags = ArgFlags::NONE,
        const Optional<Array<String>> &enum_values = { },
        const CommandLineArgumentValue &default_value = { }
    );
    
    HYP_FORCE_INLINE Iterator Find(UTF8StringView key)
    {
        return definitions.FindIf([key](const ArgParseDefinition &definition)
        {
            return definition.name == key;
        });
    }
    
    HYP_FORCE_INLINE ConstIterator Find(UTF8StringView key) const
    {
        return definitions.FindIf([key](const ArgParseDefinition &definition)
        {
            return definition.name == key;
        });
    }

    HYP_DEF_STL_BEGIN_END(
        definitions.Begin(),
        definitions.End()
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

        HYP_FORCE_INLINE explicit operator bool() const
            { return ok; }
    };

    ArgParse(const ArgParseDefinitions &definitions = { }, EnumFlags<ArgParseFlags> flags = ArgParseFlags::NONE)
        : m_definitions(definitions),
          m_flags(flags)
    {
    }

    ArgParse(const ArgParse &other)                 = default;
    ArgParse &operator=(const ArgParse &other)      = default;
    ArgParse(ArgParse &&other) noexcept             = default;
    ArgParse &operator=(ArgParse &&other) noexcept  = default;
    ~ArgParse()                                     = default;

    HYP_FORCE_INLINE void Add(
        const String &name,
        const String &shorthand = String::empty,
        EnumFlags<ArgFlags> flags = ArgFlags::NONE,
        CommandLineArgumentType type = CommandLineArgumentType::STRING,
        const CommandLineArgumentValue &default_value = { }
    )
    {
        m_definitions.Add(name, shorthand, flags, type, default_value);
    }

    HYP_FORCE_INLINE void Add(
        const String &name,
        const String &shorthand = String::empty,
        EnumFlags<ArgFlags> flags = ArgFlags::NONE,
        const Optional<Array<String>> &enum_values = { },
        const CommandLineArgumentValue &default_value = { }
    )
    {
        m_definitions.Add(name, shorthand, flags, enum_values, default_value);
    }

    HYP_API ParseResult Parse(int argc, char **argv) const;
    HYP_API ParseResult Parse(const String &command, const Array<String> &args) const;

private:
    ArgParseDefinitions         m_definitions;
    EnumFlags<ArgParseFlags>    m_flags;
};

} // namespace sys

using sys::ArgParse;

using sys::CommandLineArguments;
using sys::CommandLineArgumentValue;

} // namespace hyperion

#endif