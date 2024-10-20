/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_JSON_SOURCE_STREAM_HPP
#define HYP_JSON_SOURCE_STREAM_HPP

#include <util/json/parser/SourceFile.hpp>
#include <util/UTF8.hpp>

namespace hyperion::json {

class SourceStream
{
public:
    SourceStream(SourceFile *file);
    SourceStream(const SourceStream &other);

    SourceFile *GetFile() const { return m_file; }
    SizeType GetPosition() const { return m_position; }
    bool HasNext() const { return m_position < m_file->GetSize(); }
    utf::u32char Peek() const;
    utf::u32char Next();
    utf::u32char Next(int &pos_change);
    void GoBack(int n = 1);
    void Read(char *ptr, SizeType num_bytes);

private:
    SourceFile *m_file;
    SizeType m_position;
};

} // namespace hyperion::json

#endif
