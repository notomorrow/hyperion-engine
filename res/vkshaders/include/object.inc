#include "defines.inc"

struct Object
{
    mat4 model_matrix;
    mat4 previous_model_matrix;
    
    vec4 _pad0;
    vec4 _pad1;
    vec4 world_aabb_max;
    vec4 world_aabb_min;

    uint entity_id;
    uint scene_id;
    uint mesh_id;
    uint material_id;

    uint skeleton_id;
    uint bucket;
};

// can use up to 16 bits.
#define OBJECT_MASK_OPAQUE      (0x01)
#define OBJECT_MASK_TRANSLUCENT (0x02)
#define OBJECT_MASK_PARTICLE    (0x04)
#define OBJECT_MASK_VEGETATION  (0x08)
#define OBJECT_MASK_SKY         (0x10)
#define OBJECT_MASK_UI          (0x20)
#define OBJECT_MASK_UI_TEXT     (0x40)
#define OBJECT_MASK_UI_BUTTON   (0x80)
#define OBJECT_MASK_UI_PANEL    (0x100)

#define GET_OBJECT_BUCKET(obj) \
    ((int((obj).bucket == HYP_OBJECT_BUCKET_OPAQUE) * (OBJECT_MASK_OPAQUE)) \
        | (int((obj).bucket == HYP_OBJECT_BUCKET_TRANSLUCENT) * (OBJECT_MASK_TRANSLUCENT)) \
        | (int((obj).bucket == HYP_OBJECT_BUCKET_SKYBOX) * (OBJECT_MASK_SKY)) \
        | (int((obj).bucket == HYP_OBJECT_BUCKET_UI) * (OBJECT_MASK_UI)))

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(std430, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 1, row_major) readonly buffer ObjectBuffer
{
    Object object;
};

#endif