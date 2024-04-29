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
    UIObjectFocusState  focus_state;
    uvec2               size;
    float               border_radius;
    uint                border_flags;
};

void GetUIObjectProperties(in Object obj, out UIObjectProperties properties)
{
    properties.focus_state = obj.user_data[0];
    properties.size = uvec2(obj.user_data[1], obj.user_data[2]);
    properties.border_radius = float(obj.user_data[3] & 0xFFu);
    properties.border_flags = uint(obj.user_data[3] >> 8u) & 0xFu;
}

#endif