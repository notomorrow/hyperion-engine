#version 330 core

#include "../include/frag_output.inc"
#include "../include/depth.inc"
#include "../include/matrices.inc"

#define $SAMPLE_CNT 32
#define $PI 3.14
#define $SAMPLE_PER_PIXEL 8
#define $RADIUS 5.0
#define $GI_BOOST 1.5

uniform sampler2D u_noiseMap;  // random vectors

uniform sampler2D ColorMap;
uniform sampler2D DepthMap;
uniform sampler2D PositionMap;
uniform sampler2D NormalMap; // included in lighting.inc
uniform sampler2D DataMap;

uniform mat4 InverseViewProjMatrix;

uniform vec2 u_resolution;  // screen dimensions
uniform vec2 u_clipPlanes;       // clipping planes
uniform vec2 uTanFovs;     // tangent of field of views
uniform vec3 uLightPos;    // ligh pos in view space


uniform vec3 u_kernel[$SAMPLE_CNT];

// --------------------------------------------------
// Functions
// --------------------------------------------------
// get the view position of a sample from its depth 
// and eye information
vec3 ndc_to_view(vec2 ndc,
                 float depth,
                 vec2 clipPlanes,
                 vec2 tanFov);
vec2 view_to_ndc(vec3 view,
                 vec2 clipPlanes,
                 vec2 tanFov);




in vec2 v_texcoord0;

#define $GI_SSAO 1

void main() {
	const float ATTF = 1e-5; // attenuation factor
	vec2 st = v_texcoord0;//*u_resolution;
    vec4 t1 = texture(NormalMap, st);
    float depth = texture(DepthMap, st).r;
	//vec4 t1 = texture(sNd,st);      // read normal + depth
	vec3 t2 = texture(ColorMap,st).rgb; // colour
	vec3 n = t1.rgb*2.0-1.0; // rebuild normal
	vec3 p = positionFromDepth(InverseViewProjMatrix, v_texcoord0, depth);//ndc_to_view(v_texcoord0*2.0-1.0, depth, u_clipPlanes, uTanFovs); // get view pos
	vec3 l = uLightPos - p; // light vec
	float att = 1.0+ATTF*length(l);
	float nDotL = max(0.0,dot(normalize(l),n));
    output0 = texture(ColorMap, v_texcoord0);
    output4 = vec4(0.0, 0.0, 0.0, 0.0);
	output4.rgb = vec3(1.0);//t2*nDotL/(att*att);

#if GI_SSAO
	float occ = 0.0;
	float occCnt = 0.0;
	vec3 rvec = normalize(texture(u_noiseMap, gl_FragCoord.xy/64.0).rgb*2.0-1.0);
	for(int i=0; i<$SAMPLE_PER_PIXEL && depth < 1.0; ++i) {
		vec3 dir = reflect(u_kernel[i].xyz,rvec); // a la Crysis
		dir -= 2.0*dir*step(dot(n,dir),0.0);      // a la Starcraft
		vec3 sp = p + (dir * $RADIUS) * (depth * 1e2); // scale radius with depth
		vec2 spNdc = view_to_ndc(sp, u_clipPlanes, uTanFovs); // get sample ndc coords
		bvec4 outOfScreen = bvec4(false); // check if sample projects to screen
		outOfScreen.xy = lessThan(spNdc, vec2(-1));
		outOfScreen.zw = greaterThan(spNdc, vec2(1));
		if(any(outOfScreen)) continue;
        float depthSample = texture(DepthMap, (spNdc*0.5 + 0.5)*u_resolution).r;
		//vec4 spNd = texture(sNd,(spNdc*0.5 + 0.5)*u_resolution); // get nd data
		vec3 occEye = -sp/sp.z*(depthSample*u_clipPlanes.x+u_clipPlanes.y); // compute correct pos
		vec3 occVec = occEye - p; // vector
		float att2 = 1.0+ATTF*length(occVec); // quadratic attenuation
		occ += max(0.0,dot(normalize(occVec),n)-0.25) / (att2*att2);
		++occCnt;
	};
	output0.rgb*= occCnt > 0.0 ? vec3(1.0-occ*$GI_BOOST/occCnt) : vec3(1);
#endif
#if GI_SSDO
	vec3 gi = vec3(0.0);
	float giCnt = 0.0;
	vec3 rvec = normalize(texture(u_noiseMap, gl_FragCoord.xy/64.0).rgb*2.0-1.0);
	for(int i=0; i<$SAMPLE_PER_PIXEL && depth < 1.0; ++i) {
		vec3 dir = reflect(u_kernel[i].xyz,rvec); // a la Crysis
		dir -= 2.0*dir*step(dot(n,dir),0.0);      // a la Starcraft
		vec3 sp = p + (dir * $RADIUS) * (depth * 1e2); // scale radius with depth
		vec2 spNdc = view_to_ndc(sp, u_clipPlanes, uTanFovs); // get sample ndc coords
		bvec4 outOfScreen = bvec4(false); // check if sample projects to screen
		outOfScreen.xy = lessThan(spNdc, vec2(-1));
		outOfScreen.zw = greaterThan(spNdc, vec2(1));
		if(any(outOfScreen)) continue;
		vec2 spSt = (spNdc*0.5 + 0.5)*u_resolution;
		//vec4 spNd = texture(sNd,spSt); // get nd data
        float depthSample = texture(DepthMap, spSt).r;
		vec3 occEye = -sp/sp.z*(depthSample*u_clipPlanes.x+u_clipPlanes.y); // compute correct pos
		vec3 occVec = occEye - p; // vector
		float att2 = 1.0+ATTF*length(occVec); // quadratic attenuation
		vec3 spL = uLightPos - occEye; // sample light vec
		vec3 spKa = texture(ColorMap, spSt).rgb; // sample albedo
		vec3 spN = spNd.rgb*2.0-1.0; // sample normal
		float spAtt = 1.0+ATTF*length(spL); // quadratic attenuation
		vec3 spE = spKa*max(0.0,dot(normalize(spL),spN))/(spAtt*spAtt); // can precomp.
		float v = 1.0-max(0.0,dot(n,spN));
		gi+= spE*v*max(0.0,dot(normalize(occVec),n))/(att2*att2);
		++giCnt;
	};
	output0.rgb+= giCnt > 0.0 ? t2*gi*nDotL*$GI_BOOST/giCnt : vec3(0);
#endif // GI_SSAO
}


// --------------------------------------------------
// Functions impl.
// --------------------------------------------------
vec3 ndc_to_view(vec2 ndc,
                 float depth,
                 vec2 clipPlanes,
                 vec2 tanFov) {
	// go from [0,1] to [zNear, zFar]
	float z = depth * clipPlanes.x + clipPlanes.y;
	// view space position
	return vec3(ndc * tanFov, -1) * z;
}

vec2 view_to_ndc(vec3 view,
                 vec2 clipPlanes,
                 vec2 tanFov) {
	return (u_projMatrix * u_viewMatrix * vec4(view, 1.0)).xy;//-view.xy / (tanFov*view.z);
}



