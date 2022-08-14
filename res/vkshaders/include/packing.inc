#ifndef HYP_PACKING
#define HYP_PACKING

//#define HYP_PACK_NORMALS 1
//#define HYP_NORMAL_PACKING_0
//#define HYP_NORMAL_PACKING_1

vec4 EncodeNormal(vec3 n)
{
#if defined(HYP_PACK_NORMALS) && HYP_PACK_NORMALS
    #ifdef HYP_NORMAL_PACKING_0
        float scale = 1.7777;
        vec2 enc = n.xy / (n.z+1.0);
        enc /= scale;
        enc = enc*0.5+0.5;
        return vec4(enc,0,0);

    #else
        float p = sqrt(n.z*8+8);
        return vec4(n.xy/p + 0.5,0,0);
    #endif
#else
    return vec4(n * 0.5 + 0.5, 1.0);
#endif
}

vec3 DecodeNormal(vec4 enc)
{
#if defined(HYP_PACK_NORMALS) && HYP_PACK_NORMALS
    #ifdef HYP_NORMAL_PACKING_0
        float scale = 1.7777;
        vec3 nn =
            enc.xyz*vec3(2*scale,2*scale,0) +
            vec3(-scale,-scale,1);
        float g = 2.0 / dot(nn.xyz,nn.xyz);
        vec3 n;
        n.xy = g*nn.xy;
        n.z = g-1.0;
        return n;
    #else
        vec2 fenc = enc.xy*4-2;
        float f = dot(fenc,fenc);
        float g = sqrt(1-f/4);
        vec3 n;
        n.xy = fenc*g;
        n.z = 1-f/2;
        return n;
    #endif
#else
    return enc.xyz * 2.0 - 1.0;
#endif
}

#endif
