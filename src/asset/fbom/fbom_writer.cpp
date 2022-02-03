#include "fbom.h"
#include "../byte_writer.h"

#include <stack>

namespace hyperion {
namespace fbom {

FBOMResult FBOMWriter::WriteToByteStream(ByteWriter *writer)
{

}

FBOMResult FBOMWriter::WriteObjectType(ByteWriter *writer, FBOMType type)
{
    std::cout << "Write object tpye " << type.name << "\n";
    std::stack<const FBOMType*> type_chain;
    const FBOMType *type_ptr = &type;

    while (type_ptr != nullptr) {
        type_chain.push(type_ptr);
        type_ptr = type_ptr->extends;
    }

    // for now just writing type in place
    writer->Write(uint8_t(0x02));

    // TODO: assert length < max of uint8_t
    uint8_t typechain_length = type_chain.size();
    writer->Write(typechain_length);

    while (!type_chain.empty()) {
        std::cout << " extends " << type_chain.top()->name << "\n";
        // write string of object type (loader to use)
        writer->WriteString(type_chain.top()->name);

        // write size of the type
        writer->Write(uint64_t(type_chain.top()->size));

        type_chain.pop();
    }

    return FBOMResult(FBOMResult::FBOM_OK);
}

} // namespace fbom
} // namespace hyperion
