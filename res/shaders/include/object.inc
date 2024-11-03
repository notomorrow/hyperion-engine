#ifndef HYP_OBJECT_GLSL
#define HYP_OBJECT_GLSL

#include "defines.inc"

#define ENTITY_GPU_FLAG_NONE         0x0
#define ENTITY_GPU_FLAG_HAS_SKELETON 0x1

struct Object
{
    mat4    model_matrix;
    mat4    previous_model_matrix;
    
    vec4    _pad0;
    vec4    _pad1;
    vec4    world_aabb_max;
    vec4    world_aabb_min;

    uint    entity_index;
    uint    _unused;
    uint    material_index;
    uint    skeleton_index;

    uint    bucket;
    uint    flags;
    uint    _pad3;
    uint    _pad4;

    uvec4   user_data0;
    uvec4   user_data1;
};

#define MAX_ENTITIES_PER_INSTANCE_BATCH 60

struct EntityInstanceBatch
{
    uint    num_entities;
    uint    _pad0;
    uint    _pad1;
    uint    _pad2;

    uvec4   indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    mat4    transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

// can use up to 16 bits.
#define OBJECT_MASK_OPAQUE      (0x01)
#define OBJECT_MASK_TRANSLUCENT (0x02)
#define OBJECT_MASK_PARTICLE    (0x04)
#define OBJECT_MASK_VEGETATION  (0x08)
#define OBJECT_MASK_SKY         (0x10)
#define OBJECT_MASK_TERRAIN     (0x20)
#define OBJECT_MASK_UI          (0x40)
#define OBJECT_MASK_UI_TEXT     (0x80)
#define OBJECT_MASK_UI_BUTTON   (0x100)
#define OBJECT_MASK_UI_PANEL    (0x200)
#define OBJECT_MASK_DEBUG       (0x400)

#define GET_OBJECT_BUCKET(obj) \
    ((uint((obj).bucket == HYP_OBJECT_BUCKET_OPAQUE) * (OBJECT_MASK_OPAQUE)) \
        | (uint((obj).bucket == HYP_OBJECT_BUCKET_TRANSLUCENT) * (OBJECT_MASK_TRANSLUCENT)) \
        | (uint((obj).bucket == HYP_OBJECT_BUCKET_SKYBOX) * (OBJECT_MASK_SKY)) \
        | (uint((obj).bucket == HYP_OBJECT_BUCKET_UI) * (OBJECT_MASK_UI)))

#ifdef VERTEX_SHADER
    #define OBJECT_INDEX (entity_instance_batch.indices[gl_InstanceIndex >> 2][gl_InstanceIndex & 3])
    #define object (objects[OBJECT_INDEX])
#elif defined(FRAGMENT_SHADER)
    #define object (objects[v_object_index])
#endif

#endif