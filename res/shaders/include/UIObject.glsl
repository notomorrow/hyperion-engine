#ifndef HYP_UI_OBJECT_GLSL
#define HYP_UI_OBJECT_GLSL

#define UI_OBJECT_FOCUS_STATE_NONE      0x0
#define UI_OBJECT_FOCUS_STATE_HOVER     0x1
#define UI_OBJECT_FOCUS_STATE_PRESSED   0x2

#define UIObjectFocusState uint

struct UIObjectProperties
{
    UIObjectFocusState  focus_state;
    uvec2               size;
    float               border_radius;
};

void GetUIObjectProperties(in Object obj, out UIObjectProperties properties)
{
    properties.focus_state = obj.user_data[0];
    properties.size = uvec2(obj.user_data[1], obj.user_data[2]);
    properties.border_radius = float(obj.user_data[3] & 0xFFu);
}

#endif