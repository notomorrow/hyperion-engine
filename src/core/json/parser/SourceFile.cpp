/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/json/parser/SourceFile.hpp>

#include <core/debug/Debug.hpp>

#include <cstring>

namespace hyperion::json {

SourceFile::SourceFile()
    : m_filepath("??"),
      m_position(0)
{
}

SourceFile::SourceFile(const String& filepath, SizeType size)
    : m_filepath(filepath),
      m_position(0)
{
    m_buffer.SetSize(size);
}

SourceFile::SourceFile(const SourceFile& other)
    : m_filepath(other.m_filepath),
      m_buffer(other.m_buffer.Copy()),
      m_position(other.m_position)
{
}

SourceFile& SourceFile::operator=(const SourceFile& other)
{
    if (&other == this)
    {
        return *this;
    }

    m_buffer = other.m_buffer.Copy();
    m_position = other.m_position;
    m_filepath = other.m_filepath;

    return *this;
}

SourceFile::~SourceFile() = default;

void SourceFile::ReadIntoBuffer(const ByteBuffer& input_buffer)
{
    AssertThrow(m_buffer.Size() >= input_buffer.Size());

    // make sure we have enough space in the buffer
    AssertThrowMsg(m_position + input_buffer.Size() <= m_buffer.Size(), "not enough space in buffer");

    for (SizeType i = 0; i < input_buffer.Size(); i++)
    {
        m_buffer.Data()[m_position++] = input_buffer.Data()[i];
    }
}

void SourceFile::ReadIntoBuffer(const ubyte* data, SizeType size)
{
    AssertThrow(m_buffer.Size() >= size);

    // make sure we have enough space in the buffer
    AssertThrowMsg(m_position + size <= m_buffer.Size(), "not enough space in buffer");

    for (SizeType i = 0; i < size; i++)
    {
        m_buffer.Data()[m_position++] = data[i];
    }
}

} // namespace hyperion::json
