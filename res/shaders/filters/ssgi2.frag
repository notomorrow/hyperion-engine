#version 330 core
in vec2 v_texcoord0; 
#include "../include/matrices.inc"
#include "../include/frag_output.inc"

uniform sampler2D DepthMap;
uniform sampler2D NormalMap;
uniform sampler2D ColorMap;
uniform sampler2D u_noiseMap;
uniform mat4 InverseViewProjMatrix;
uniform vec2 u_resolution;
#define $CAP_MIN_DISTANCE 0.0001
#define $CAP_MAX_DISTANCE 0.005
const vec2 poisson16[] = vec2[](    // These are the Poisson Disk Samples
                                vec2( -0.94201624,  -0.39906216 ),
                                vec2(  0.94558609,  -0.76890725 ),
                                vec2( -0.094184101, -0.92938870 ),
                                vec2(  0.34495938,   0.29387760 ),
                                vec2( -0.91588581,   0.45771432 ),
                                vec2( -0.81544232,  -0.87912464 ),
                                vec2( -0.38277543,   0.27676845 ),
                                vec2(  0.97484398,   0.75648379 ),
                                vec2(  0.44323325,  -0.97511554 ),
                                vec2(  0.53742981,  -0.47373420 ),
                                vec2( -0.26496911,  -0.41893023 ),
                                vec2(  0.79197514,   0.19090188 ),
                                vec2( -0.24188840,   0.99706507 ),
                                vec2( -0.81409955,   0.91437590 ),
                                vec2(  0.19984126,   0.78641367 ),
                                vec2(  0.14383161,  -0.14100790 )
                                );
float unpack_depth(vec4 rgba_depth){
    const vec4 bit_shift =
        vec4(1.0/(256.0*256.0*256.0)
            , 1.0/(256.0*256.0)
            , 1.0/256.0
            , 1.0);
    float depth = dot(rgba_depth, bit_shift);
    return depth;
}
vec3 getPosition(vec2 uv, float depth) {
        
        vec4 ray = inverse(u_projMatrix) * vec4((uv.x - 0.5) *2., ((uv.y - 0.5) * 2.), 1.0, 1.0);
        ray *= depth;
        return ray.xyz;
}
    vec2 getRandom(in vec2 uv)
{
return normalize(texture(u_noiseMap, uv / 0.1).xy * 2.0 - 1.0);
}
vec3 readNormal(in vec2 coord)  
{  
        return normalize(texture(NormalMap, coord).xyz*2.0  - 1.0);  
}
//Ambient Occlusion form factor:
    float aoFF(in vec3 ddiff,in vec3 cnorm, in float c1, in float c2){
            vec3 vv = normalize(ddiff);
            float rd = length(ddiff);
            return (1.0-clamp(dot(readNormal(v_texcoord0+vec2(c1,c2)),-vv),0.0,1.0)) *
            clamp(dot( cnorm,vv ),0.0,1.0)* 
                    (1.0 - 1.0/sqrt(1.0/(rd*rd) + 1.0));
    }

//GI form factor:
    float giFF(in vec3 ddiff,in vec3 cnorm, in float c1, in float c2){
            vec3 vv = normalize(ddiff);
            float rd = length(ddiff);
            return 1.0*clamp(dot(readNormal(v_texcoord0+vec2(c1,c2)),vv),0.0,1.0)*
                        clamp(dot( cnorm,vv ),0.0,1.0)/
                        (rd*rd+1.0);  
    }


float threshold(in float thr1, in float thr2 , in float val) {
    if (val < thr1) {return 0.0;}
    if (val > thr2) {return 1.0;}
    return val;
}

// averaged pixel intensity from 3 color channels
float avg_intensity(in vec4 pix) {
    return (pix.r + pix.g + pix.b)/3.;
}

vec4 get_pixel(in vec2 coords, in float dx, in float dy) {
    return texture(ColorMap,coords + vec2(dx, dy));
}

// returns pixel color
float IsEdge(in vec2 coords){
    float dxtex = 1.0 / u_resolution.x /*image width*/;
    float dytex = 1.0 / u_resolution.y /*image height*/;
    float pix[9];
    int k = -1;
    float delta;

    // read neighboring pixel intensities
    for (int i=-1; i<2; i++) {
    for(int j=-1; j<2; j++) {
    k++;
    pix[k] = avg_intensity(get_pixel(coords,float(i)*dxtex,
                                            float(j)*dytex));
    }
    }

    // average color differences around neighboring pixels
    delta = (abs(pix[1]-pix[7])+
            abs(pix[5]-pix[3]) +
            abs(pix[0]-pix[8])+
            abs(pix[2]-pix[6])
            )/4.;

    return threshold(0.2,0.7,clamp(2.0*delta,0.0,1.0));
}

void main()                                  
{                                            
    vec3 gi = vec3(0.0,0.0,0.0);
    output0 = texture(ColorMap, v_texcoord0);
    float depth = texture(DepthMap, v_texcoord0).r;
    vec3 p = getPosition(v_texcoord0, depth);
    //float edge = 1.0-IsEdge(v_texcoord0);
    vec3 normals = normalize(texture(NormalMap, v_texcoord0).rgb * 2.0 - 1.0);
    vec2 fres = vec2(1.0/u_resolution.x,1.0/u_resolution.y);
    vec4 vpPos = vec4(p, 1.0);
    vec3 random = normalize(texture(u_noiseMap, v_texcoord0*50.0).rgb) * 2.0 - 1.0;
        float incx = 1.0/40.0;
        float incy = 1.0/40.0;
        float pw = incx;
        float ph = incy;

        float occlusion;
        if (depth < 1.0) {
            float result = 0.0;
            float ao;
            int iterations = 24;
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            float npw = (x + poisson16[x].x)*(1.0-depth);
            float nph = (y + poisson16[y].y)*(1.0-depth);
            float sampleDepth = unpack_depth(texture(DepthMap, v_texcoord0+vec2(npw,nph)));
            vec3 ddiff = getPosition(v_texcoord0+vec2(npw,nph), sampleDepth);
            //ao += aoFF(ddiff,normals,npw,nph);
            gi += giFF(ddiff,normals,npw,nph) * texture(ColorMap, v_texcoord0+vec2(-npw,0)).rgb;
        }
    }

        ao/=(24.);
        gi/=(24.);
        
    // ao *= edge;
        float strength = 1.0;
        result = 1.0-strength * ao;
        result = mix(0.1, 1.0, result*result);
        output4 = vec4(gi, 0.0);//((vec4(result, result, result, 0.0) ) ) ;//+ vec4(gi, 1.0);
    }
}
