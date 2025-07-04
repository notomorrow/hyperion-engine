/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/xml/SAXParser.hpp>

#include <core/io/BufferedByteReader.hpp>

namespace hyperion {
namespace xml {

SAXParser::SAXParser(SAXHandler* handler)
    : m_handler(handler)
{
}

SAXParser::Result SAXParser::Parse(const FilePath& filepath)
{
    FileBufferedReaderSource source { filepath };
    BufferedReader reader { &source };

    return Parse(&reader);
}

SAXParser::Result SAXParser::Parse(BufferedReader* reader)
{
    if (reader == nullptr)
    {
        return Result(Result::SRT_ERR, "Reader was null");
    }

    if (!reader->IsOpen())
    {
        return Result(Result::SRT_ERR, "File could not be read.");
    }

    bool is_reading = false,
         is_opening = false,
         is_closing = false,
         in_element = false,
         in_comment = false,
         in_characters = false,
         in_header = false,
         in_attributes = false,
         in_attribute_value = false,
         in_attribute_name = false;

    utf::u32char last_char = utf::u32char(-1);
    String element_str, comment_str, value_str;
    Array<Pair<String, String>> attribs;

    reader->ReadChars([&](char ch)
        {
            if (ch != '\t' && ch != '\n')
            {
                if (ch == '<')
                {
                    element_str.Clear();
                    in_characters = false;

                    if (!value_str.Empty())
                    {
                        m_handler->Characters(value_str);
                    }

                    is_opening = true;
                    is_reading = true;
                    in_element = true;
                    in_attributes = false;
                    is_closing = false;
                    value_str.Clear();
                    attribs.Clear();
                }
                else if (ch == '!' && in_element)
                {
                    in_comment = true;
                    comment_str = "";
                }
                else if (ch == '?' && in_element)
                {
                    in_header = true;
                }
                else if (ch == '/' && (in_element || (in_attributes && !in_attribute_value)))
                {
                    is_opening = false;
                    is_closing = true;
                }
                else if (ch == '>')
                {
                    in_characters = true;
                    if (in_comment)
                    {
                        in_comment = false;
                        m_handler->Comment(comment_str);
                    }
                    else if (in_header)
                    {
                        in_header = false;
                    }
                    else
                    {
                        if (is_opening || last_char == '/')
                        {
                            AttributeMap locals;

                            for (auto& attr : attribs)
                            {
                                if (!attr.first.Empty())
                                {
                                    locals[attr.first.ToLower()] = attr.second;
                                }
                            }

                            m_handler->Begin(element_str, locals);
                            is_opening = false;
                        }

                        if (is_closing)
                        {
                            m_handler->End(element_str);
                        }

                        in_attributes = false;
                        in_element = false;
                        is_closing = false;
                        is_reading = false;

                        attribs.Clear();
                    }
                }
                else
                {
                    if (!in_comment && !in_header)
                    {
                        if (is_reading)
                        {
                            if (in_element)
                            {
                                if (ch == ' ')
                                {
                                    in_element = false;
                                    in_attributes = true;
                                    attribs.PushBack({ "", "" });
                                }
                                else
                                {
                                    element_str += ch;
                                }
                            }
                            else if (in_attributes && is_opening)
                            {
                                if (!in_attribute_value && ch == ' ')
                                {
                                    attribs.PushBack({ "", "" });
                                }
                                else if (ch == '\"' && last_char != '\\')
                                {
                                    in_attribute_value = !in_attribute_value;
                                }
                                else if (ch != '\\')
                                {
                                    auto& last = attribs.Back();
                                    if (!in_attribute_value && ch != '=')
                                    {
                                        last.first += ch;
                                    }
                                    else if (in_attribute_value)
                                    {
                                        last.second += ch;
                                    }
                                }
                            }
                        }
                        else if (in_characters)
                        {
                            if (ch != ' ' || (last_char != ' ' && (last_char != '\n' && last_char != '<')))
                            {
                                value_str += ch;
                            }
                        }
                    }
                    else if (in_comment && ch != '-')
                    {
                        comment_str += ch;
                    }
                }
            }
            last_char = ch;
        });

    return { Result::SRT_OK };
}

} // namespace xml
} // namespace hyperion
