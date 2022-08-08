#ifndef HYP_PACKING
#define HYP_PACKING

#define HYP_PACK_NORMALS 0

vec4 EncodeNormal(vec3 n)
{
#if HYP_PACK_NORMALS
    float scale = 1.7777;
    vec2 enc = n.xy / (n.z+1.0);
    enc /= scale;
    enc = enc*0.5+0.5;
    return vec4(enc,0,0);
#else
    return vec4(n * 0.5 + 0.5, 1.0);
#endif
}

vec3 DecodeNormal(vec4 enc)
{
#if HYP_PACK_NORMALS
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
    return enc.xyz * 2.0 - 1.0;
#endif
}

#endif
