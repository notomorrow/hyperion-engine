#ifndef HYP_UI_OBJECT_GLSL
#define HYP_UI_OBJECT_GLSL

#define UI_OBJECT_FOCUS_STATE_NONE      0x0
#define UI_OBJECT_FOCUS_STATE_HOVER     0x1
#define UI_OBJECT_FOCUS_STATE_PRESSED   0x2

#define UIObjectFocusState uint

struct UIObjectProperties
{
    UIObjectFocusState  focus_state;
};

void GetUIObjectProperties(in Object obj, out UIObjectProperties properties)
{
    properties.focus_state = obj.user_data[0];
}

#endif