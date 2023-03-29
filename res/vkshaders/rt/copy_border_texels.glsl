#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#if DEPTH
    #define GROUP_SIZE 16
#else
    #define GROUP_SIZE 8
#endif

layout(local_size_x = GROUP_SIZE, local_size_y = GROUP_SIZE, local_size_z = 1) in;

#include "../include/rt/probe/probe_uniforms.inc"

#if DEPTH
    #define PROBE_SIDE_LENGTH PROBE_SIDE_LENGTH_DEPTH
    #define OUTPUT_IMAGE output_depth
    #define OUTPUT_IMAGE_DIMENSIONS probe_system.image_dimensions.zw
#else
    #define PROBE_SIDE_LENGTH PROBE_SIDE_LENGTH_IRRADIANCE
    #define OUTPUT_IMAGE output_irradiance
    #define OUTPUT_IMAGE_DIMENSIONS probe_system.image_dimensions.xy
#endif

#define PROBE_SIDE_LENGTH_BORDER (PROBE_SIDE_LENGTH + probe_system.probe_border.x)

layout(std140, set = 0, binding = 9) uniform ProbeSystem {
    ProbeSystemUniforms probe_system;
};

layout(std140, set = 0, binding = 10) buffer ProbeRayDataBuffer {
    ProbeRayData probe_rays[];
};

#include "../include/rt/probe/shared.inc"

layout(set = 0, binding = 11, rgba16f) uniform image2D output_irradiance;
layout(set = 0, binding = 12, rg16f) uniform image2D output_depth;

#if DEPTH
const ivec4 g_offsets[68] = ivec4[](
    ivec4(16, 1, 1, 0),
    ivec4(15, 1, 2, 0),
    ivec4(14, 1, 3, 0),
    ivec4(13, 1, 4, 0),
    ivec4(12, 1, 5, 0),
    ivec4(11, 1, 6, 0),
    ivec4(10, 1, 7, 0),
    ivec4(9, 1, 8, 0),
    ivec4(8, 1, 9, 0),
    ivec4(7, 1, 10, 0),
    ivec4(6, 1, 11, 0),
    ivec4(5, 1, 12, 0),
    ivec4(4, 1, 13, 0),
    ivec4(3, 1, 14, 0),
    ivec4(2, 1, 15, 0),
    ivec4(1, 1, 16, 0),
    ivec4(16, 16, 1, 17),
    ivec4(15, 16, 2, 17),
    ivec4(14, 16, 3, 17),
    ivec4(13, 16, 4, 17),
    ivec4(12, 16, 5, 17),
    ivec4(11, 16, 6, 17),
    ivec4(10, 16, 7, 17),
    ivec4(9, 16, 8, 17),
    ivec4(8, 16, 9, 17),
    ivec4(7, 16, 10, 17),
    ivec4(6, 16, 11, 17),
    ivec4(5, 16, 12, 17),
    ivec4(4, 16, 13, 17),
    ivec4(3, 16, 14, 17),
    ivec4(2, 16, 15, 17),
    ivec4(1, 16, 16, 17),
    ivec4(1, 16, 0, 1),
    ivec4(1, 15, 0, 2),
    ivec4(1, 14, 0, 3),
    ivec4(1, 13, 0, 4),
    ivec4(1, 12, 0, 5),
    ivec4(1, 11, 0, 6),
    ivec4(1, 10, 0, 7),
    ivec4(1, 9, 0, 8),
    ivec4(1, 8, 0, 9),
    ivec4(1, 7, 0, 10),
    ivec4(1, 6, 0, 11),
    ivec4(1, 5, 0, 12),
    ivec4(1, 4, 0, 13),
    ivec4(1, 3, 0, 14),
    ivec4(1, 2, 0, 15),
    ivec4(1, 1, 0, 16),
    ivec4(16, 16, 17, 1),
    ivec4(16, 15, 17, 2),
    ivec4(16, 14, 17, 3),
    ivec4(16, 13, 17, 4),
    ivec4(16, 12, 17, 5),
    ivec4(16, 11, 17, 6),
    ivec4(16, 10, 17, 7),
    ivec4(16, 9, 17, 8),
    ivec4(16, 8, 17, 9),
    ivec4(16, 7, 17, 10),
    ivec4(16, 6, 17, 11),
    ivec4(16, 5, 17, 12),
    ivec4(16, 4, 17, 13),
    ivec4(16, 3, 17, 14),
    ivec4(16, 2, 17, 15),
    ivec4(16, 1, 17, 16),
    ivec4(16, 16, 0, 0),
    ivec4(1, 16, 17, 0),
    ivec4(16, 1, 0, 17),
    ivec4(1, 1, 17, 17)
);
#else
const ivec4 g_offsets[36] = ivec4[](
    ivec4(8, 1, 1, 0),
    ivec4(7, 1, 2, 0),
    ivec4(6, 1, 3, 0),
    ivec4(5, 1, 4, 0),
    ivec4(4, 1, 5, 0),
    ivec4(3, 1, 6, 0),
    ivec4(2, 1, 7, 0),
    ivec4(1, 1, 8, 0),
    ivec4(8, 8, 1, 9),
    ivec4(7, 8, 2, 9),
    ivec4(6, 8, 3, 9),
    ivec4(5, 8, 4, 9),
    ivec4(4, 8, 5, 9),
    ivec4(3, 8, 6, 9),
    ivec4(2, 8, 7, 9),
    ivec4(1, 8, 8, 9),
    ivec4(1, 8, 0, 1),
    ivec4(1, 7, 0, 2),
    ivec4(1, 6, 0, 3),
    ivec4(1, 5, 0, 4),
    ivec4(1, 4, 0, 5),
    ivec4(1, 3, 0, 6),
    ivec4(1, 2, 0, 7),
    ivec4(1, 1, 0, 8),
    ivec4(8, 8, 9, 1),
    ivec4(8, 7, 9, 2),
    ivec4(8, 6, 9, 3),
    ivec4(8, 5, 9, 4),
    ivec4(8, 4, 9, 5),
    ivec4(8, 3, 9, 6),
    ivec4(8, 2, 9, 7),
    ivec4(8, 1, 9, 8),
    ivec4(8, 8, 0, 0),
    ivec4(1, 8, 9, 0),
    ivec4(8, 1, 0, 9),
    ivec4(1, 1, 9, 9)
);
#endif


// ------------------------------------------------------------------
// FUNCTIONS --------------------------------------------------------
// ------------------------------------------------------------------

void copy_texel(ivec2 current_coord, uint index)
{
    ivec2 src_coord = current_coord + g_offsets[index].xy;
    ivec2 dst_coord = current_coord + g_offsets[index].zw;

    imageStore(OUTPUT_IMAGE, dst_coord, imageLoad(OUTPUT_IMAGE, src_coord));
}

// ------------------------------------------------------------------
// MAIN -------------------------------------------------------------
// ------------------------------------------------------------------

void main()
{
    const ivec2 current_coord = (ivec2(gl_WorkGroupID.xy) * ivec2(PROBE_SIDE_LENGTH_BORDER)) + ivec2(1);

    copy_texel(current_coord, gl_LocalInvocationIndex);

    if (gl_LocalInvocationIndex < 4)
        copy_texel(current_coord, (GROUP_SIZE * GROUP_SIZE) + gl_LocalInvocationIndex);
}


#if 0
void main()
{
    // TERRIBLE WAY OF DOING THIS.  just for testing.
    for (int x = int(gl_GlobalInvocationID.x); x < int(gl_GlobalInvocationID.x) + GROUP_SIZE; x++) {
        for (int y = int(gl_GlobalInvocationID.y); y < int(gl_GlobalInvocationID.y) + GROUP_SIZE; y++) {
            const ivec2 coord = ivec2(x, y);

            if (any(greaterThanEqual(coord, ivec2(OUTPUT_IMAGE_DIMENSIONS.xy)))) {
                return;
            }

            if (coord.x == 0 || coord.y == 0 || coord.x == int(OUTPUT_IMAGE_DIMENSIONS.x - 1) || coord.y == int(OUTPUT_IMAGE_DIMENSIONS.y - 1)) {
                continue;
            }

            if (coord.x % (PROBE_SIDE_LENGTH + PROBE_BORDER_SIZE) == 0) {
                vec4 value = imageLoad(OUTPUT_IMAGE, ivec2(coord.x - 1, coord.y));
                imageStore(OUTPUT_IMAGE, coord, value);

                continue;
            }

            if (coord.x % (PROBE_SIDE_LENGTH + PROBE_BORDER_SIZE) == 1) {
                vec4 value = imageLoad(OUTPUT_IMAGE, ivec2(coord.x + 1, coord.y));
                imageStore(OUTPUT_IMAGE, coord, value);

                continue;
            }

            if (coord.y % (PROBE_SIDE_LENGTH + PROBE_BORDER_SIZE) == 0) {
                vec4 value = imageLoad(OUTPUT_IMAGE, ivec2(coord.x, coord.y - 1));
                imageStore(OUTPUT_IMAGE, coord, value);

                continue;
            }

            if (coord.y % (PROBE_SIDE_LENGTH + PROBE_BORDER_SIZE) == 1) {
                vec4 value = imageLoad(OUTPUT_IMAGE, ivec2(coord.x, coord.y + 1));
                imageStore(OUTPUT_IMAGE, coord, value);

                continue;
            }
        }
    }
}
#endif


// mappings2 = ((len) => {
//     let values = []
//     for (let x = len; x >= 1; x--) {
//         let y = 1;

//         let diff = len - x;
//         let dstX = (diff + 1);
//         let dstY = 0;

//         values.push([x, y, dstX, dstY]);
//     }
//     for (let x = len; x >= 1; x--) {
//         let y = len;

//         let diff = len - x;
//         let dstX = (diff + 1);
//         let dstY = len + 1;

//         values.push([x, y, dstX, dstY]);
//     }
//     for (let y = len; y >= 1; y--) {
//         let x = 1;

//         let diff = len - y;
//         let dstY = (diff + 1);
//         let dstX = 0;

//         values.push([x, y, dstX, dstY]);
//     }
//     for (let y = len; y >= 1; y--) {
//         let x = len;

//         let diff = len - y;
//         let dstY = (diff + 1);
//         let dstX = len + 1;

//         values.push([x, y, dstX, dstY]);
//     }

//     // corners
//     values.push([len, len, 0, 0]);
//     values.push([1, len, len + 1, 0]);
//     values.push([len, 1, 0, len + 1]);
//     values.push([1, 1, len + 1, len + 1]);

//     const mappingsText = values.map((v) =>
//         `    ivec4(${v.join(', ')})`).join(',\n')

//     return (
//         `const ivec4 g_offsets[${values.length}] = ivec4[](${mappingsText});
//         `
//     )
// })(32)