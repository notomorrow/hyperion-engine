#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct MeshDescription
{
    uint64_t vertex_buffer_address;
    uint64_t index_buffer_address;
    uint32_t material_index;
};