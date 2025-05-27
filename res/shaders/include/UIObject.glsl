#ifndef HYP_UI_OBJECT_GLSL
#define HYP_UI_OBJECT_GLSL

#define UOFS_NONE      0x0
#define UOFS_HOVER     0x1
#define UOFS_PRESSED   0x2
#define UOFS_TOGGLED   0x4
#define UOFS_FOCUSED   0x8

#define UIObjectFocusState uint

#define UOB_NONE           0x0
#define UOB_TOP            0x1
#define UOB_LEFT           0x2
#define UOB_BOTTOM         0x4
#define UOB_RIGHT          0x8

#define UIObjectBorderFlags uint

struct UIObjectProperties
{
    uvec2               size;
    vec4                clamped_aabb;
    float               border_radius;
    uint                border_flags;
    UIObjectFocusState  focus_state;
};

struct UIEntityInstanceBatch
{
    EntityInstanceBatch batch;
    vec4                texcoords[MAX_ENTITIES_PER_INSTANCE_BATCH];
    vec4                offsets[MAX_ENTITIES_PER_INSTANCE_BATCH];
    vec4                sizes[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

void GetUIObjectProperties(in Object obj, out UIObjectProperties properties)
{
    properties.border_radius = float(obj.user_data0[0] & 0xFFu);
    properties.border_flags = uint(obj.user_data0[0] >> 8u) & 0xFu;
    properties.focus_state = uint(obj.user_data0[0] >> 16u) & 0xFFu;
    properties.size = uvec2(obj.user_data0[2], obj.user_data0[3]);
    properties.clamped_aabb = vec4(
        uintBitsToFloat(obj.user_data1[0]),
        uintBitsToFloat(obj.user_data1[1]),
        uintBitsToFloat(obj.user_data1[2]),
        uintBitsToFloat(obj.user_data1[3])
    );
}

#endif