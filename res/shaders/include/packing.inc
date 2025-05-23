#ifndef HYP_PACKING
#define HYP_PACKING

//#define HYP_PACK_NORMALS 1
#define HYP_NORMAL_PACKING_0
// #define HYP_NORMAL_PACKING_1
//#define HYP_NORMAL_PACKING_2

vec2 PackNormalVec2(vec3 n)
{
#ifdef HYP_NORMAL_PACKING_0
    float scale = 1.7777;
    vec2 enc = n.xy / (n.z+1.0);
    enc /= scale;
    enc = enc*0.5+0.5;
    return enc;
#elif defined(HYP_NORMAL_PACKING_1)
    float p = sqrt(n.z*8+8);
    return n.xy / p + 0.5;
#elif defined(HYP_NORMAL_PACKING_2)
    return (vec2(atan(n.x, n.y) / 3.141592654, n.z) + 1.0) * 0.5;
#else
    #error No packing mode defined
#endif
}

vec3 UnpackNormalVec2(vec2 enc)
{
#ifdef HYP_NORMAL_PACKING_0
    float scale = 1.7777;
    vec3 nn =
        vec3(enc.xy, 0.0) * vec3(2*scale,2*scale,0) +
        vec3(-scale,-scale,1);
    float g = 2.0 / dot(nn.xyz,nn.xyz);
    vec3 n;
    n.xy = g*nn.xy;
    n.z = g-1.0;
    return n;
#elif defined(HYP_NORMAL_PACKING_1)
    vec2 fenc = enc.xy*4-2;
    float f = dot(fenc,fenc);
    float g = sqrt(1-f/4);
    vec3 n;
    n.xy = fenc*g;
    n.z = 1-f/2;
    return n;
#elif defined(HYP_NORMAL_PACKING_2)
    vec2 ang = enc*2-1;
    vec2 scth = vec2(sin(ang.x * 3.141592654), cos(ang.x * 3.141592654));
    vec2 scphi = vec2(sqrt(1.0 - ang.y * ang.y), ang.y);
    return vec3(scth.y*scphi.x, scth.x*scphi.x, scphi.y);
#else
    #error No packing mode defined
#endif
}

vec4 EncodeNormal(vec3 n)
{
#if defined(HYP_PACK_NORMALS) && HYP_PACK_NORMALS
    return vec4(PackNormalVec2(n), 0.0, 0.0);
#else
    return vec4(n * 0.5 + 0.5, 1.0);
#endif
}

vec3 DecodeNormal(vec4 enc)
{
#if defined(HYP_PACK_NORMALS) && HYP_PACK_NORMALS
    return UnpackNormalVec2(enc.xy);
#else
    return enc.xyz * 2.0 - 1.0;
#endif
}

#endif
