#include <asset/serialization/fbom/FBOMData.hpp>
#include <core/lib/CMemory.hpp>

namespace hyperion::v2::fbom {

const FBOMData FBOMData::UNSET = FBOMData();

FBOMData::FBOMData()
    : type(FBOMUnset())
{
}

FBOMData::FBOMData(const FBOMType &type)
    : bytes(type.size),
      type(type)
{
}

FBOMData::FBOMData(const FBOMType &type, ByteBuffer &&byte_buffer)
    : bytes(std::move(byte_buffer)),
      type(type)
{
}

FBOMData::FBOMData(const FBOMData &other)
    : type(other.type),
      bytes(other.bytes)
{
}

FBOMData &FBOMData::operator=(const FBOMData &other)
{
    type = other.type;
    bytes = other.bytes;

    return *this;
}

FBOMData::FBOMData(FBOMData &&other) noexcept
    : bytes(std::move(other.bytes)),
      type(std::move(other.type))
{
    other.type = FBOMUnset();
}

FBOMData &FBOMData::operator=(FBOMData &&other) noexcept
{
    bytes = std::move(other.bytes);
    type = std::move(other.type);

    other.type = FBOMUnset();

    return *this;
}

FBOMData::~FBOMData() = default;

SizeType FBOMData::ReadBytes(SizeType n, void *out) const
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(n <= bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, bytes.Size());
    Memory::Copy(out, bytes.Data(), to_read);
    return to_read;
}

ByteBuffer FBOMData::ReadBytes() const
{
    return bytes;
}

ByteBuffer FBOMData::ReadBytes(SizeType n) const
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(n <= bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, bytes.Size());

    return ByteBuffer(to_read, bytes.Data());
}

void FBOMData::SetBytes(SizeType n, const void *data)
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(n <= type.size, "Attempt to insert data past size max size of object");
    }

    bytes.SetData(n, data);
}

} // namespace hyperion::v2::fbom
