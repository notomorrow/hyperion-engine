#ifndef HYP_PHYSICAL_CAMERA_GLSL
#define HYP_PHYSICAL_CAMERA_GLSL

/* Physical camera */
const float aperture = 16.0;
const float shutter = 1.0/100.0;
const float sensitivity = 100.0;
const float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
const float exposure = 1.0 / (1.2 * pow(2.0, ev100));

#endif