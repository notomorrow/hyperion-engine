#include "fbom.h"
#include "../byte_writer.h";

#include <stack>

namespace hyperion {
namespace fbom {

void FBOMObject::WriteToByteStream(ByteWriter *out) const
{
    out->Write(uint8_t(FBOM_OBJECT_START));

    // write typechain
    FBOMWriter::WriteObjectType(out, m_object_type);

    // add all properties
    for (auto &it : properties) {
        out->Write(uint8_t(FBOM_DEFINE_PROPERTY));

        // write property name
        out->WriteString(it.first);

        // write property type
        FBOMWriter::WriteObjectType(out, it.second.GetType());

        // write out size of object
        // TODO: if not unbounded type, assert that size of data matches type size
        size_t total_size = it.second.TotalSize();
        // TODO: assert total_size is less than max value of uint32_t

        out->Write(uint32_t(total_size));

        // now write raw bytes
        unsigned char *tmp = new unsigned char[total_size];

        it.second.ReadBytes(total_size, tmp);
        out->Write(tmp, total_size);

        delete[] tmp;
    }

    // now write out all child node

    for (auto &node : nodes) {
        soft_assert_continue(node != nullptr);

        node->WriteToByteStream(out);
    }

    out->Write(uint8_t(FBOM_OBJECT_END));
}

} // namespace fbom
} // namespace hyperion
