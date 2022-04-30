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
		vec4 rval = RGBA8ToVec4Raw(cur_val);
		rval.xyz = (rval.xyz * rval.w); 	// Denormalize
		vec4 curValF = rval + rgba; 			// Add new value
		curValF.xyz /= (curValF.w); 		// Renormalize
		new_val = Vec4ToRGBA8Raw(curValF);
    }
}