#ifndef HYP_UI_OBJECT_GLSL
#define HYP_UI_OBJECT_GLSL

#define UI_OBJECT_FOCUS_STATE_NONE      0x0
#define UI_OBJECT_FOCUS_STATE_HOVER     0x1
#define UI_OBJECT_FOCUS_STATE_PRESSED   0x2
#define UI_OBJECT_FOCUS_STATE_TOGGLED   0x4

#define UIObjectFocusState uint

#define UI_OBJECT_BORDER_NONE           0x0
#define UI_OBJECT_BORDER_TOP            0x1
#define UI_OBJECT_BORDER_LEFT           0x2
#define UI_OBJECT_BORDER_BOTTOM         0x4
#define UI_OBJECT_BORDER_RIGHT          0x8

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