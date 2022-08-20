#include <asset/serialization/fbom/FBOMData.hpp>

namespace hyperion::v2::fbom {

const FBOMData FBOMData::UNSET = FBOMData();

FBOMData::FBOMData()
    : type(FBOMUnset()),
      data_size(0),
      raw_data(nullptr)
{
}

FBOMData::FBOMData(const FBOMType &type)
    : type(type),
      data_size(0),
      raw_data(nullptr)
{
}

FBOMData::FBOMData(const FBOMData &other)
    : type(other.type),
      data_size(other.data_size)
{
    if (data_size == 0) {
        raw_data = nullptr;
    } else {
        raw_data = new unsigned char[data_size];
        std::memcpy(raw_data, other.raw_data, data_size);
    }
}

FBOMData &FBOMData::operator=(const FBOMData &other)
{
    type = other.type;

    if (raw_data != nullptr && other.data_size > data_size) {
        delete[] raw_data;
        raw_data = nullptr;
    }

    data_size = other.data_size;

    if (raw_data == nullptr) {
        raw_data = new unsigned char[data_size];
    }

    std::memcpy(raw_data, other.raw_data, data_size);

    return *this;
}

FBOMData::FBOMData(FBOMData &&other) noexcept
    : raw_data(other.raw_data),
      type(other.type),
      data_size(other.data_size)
{
    other.raw_data = nullptr;
    other.data_size = 0;
    other.type = FBOMUnset();
}

FBOMData &FBOMData::operator=(FBOMData &&other) noexcept
{
    if (raw_data != nullptr) {
        delete[] raw_data;
    }

    raw_data = other.raw_data;
    data_size = other.data_size;
    type = other.type;

    other.raw_data = nullptr;
    other.type = FBOMUnset();
    other.data_size = 0;

    return *this;
}

FBOMData::~FBOMData()
{
    if (raw_data != nullptr) {
        delete[] raw_data;
    }
}

void FBOMData::ReadBytes(SizeType n, void *out) const
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(n <= type.size, "Attempt to read past max size of object");
    }

    size_t to_read = MathUtil::Min(n, data_size);
    std::memcpy(out, raw_data, to_read);
}

void FBOMData::SetBytes(SizeType n, const void *data)
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(n <= type.size, "Attempt to insert data past size max size of object");
    }

    if (raw_data != nullptr && n > data_size) {
        delete[] raw_data;
        raw_data = nullptr;
    }

    data_size = n;

    if (raw_data == nullptr) {
        raw_data = new unsigned char[data_size];
    }

    std::memcpy(raw_data, data, data_size);
}

} // namespace hyperion::v2::fbom
