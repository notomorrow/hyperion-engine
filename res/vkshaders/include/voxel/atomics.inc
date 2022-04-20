void AtomicSwap(uint idx, uint new_val)
{
    atomicExchange(uOctree[idx].x, new_val);
}

void AtomicSwapRGBA8(uint idx, uint val)
{
    uint prev_val = 0, cur_val, new_val = val;
    vec4 rgba = RGBA8ToVec4Raw(val);
    
    while ((cur_val = atomicCompSwap(uOctree[idx].y, prev_val, new_val)) != prev_val) {
        prev_val = cur_val;
        vec4 prev_rgba = RGBA8ToVec4Raw(prev_val);
        prev_rgba.xyz *= prev_rgba.w;
        vec4 cur_rgba = prev_rgba + rgba;
        cur_rgba.xyz /= cur_rgba.w;
        new_val = Vec4ToRGBA8Raw(cur_rgba);
    }
}