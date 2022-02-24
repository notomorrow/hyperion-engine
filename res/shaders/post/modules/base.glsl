#define $POST_PROCESS_COMPUTE 0
#define $POST_PROCESS_EQL 0
#define $POST_PROCESS_ADD 1
#define $POST_PROCESS_MUL 2

struct PostProcessData {
	vec2 texCoord;
	vec2 resolution;
	vec3 worldPosition;
	vec3 cameraPosition;
	
	float exposure;
	
	float depth;
	float linearDepth;
	
	mat4 projMatrix;
	mat4 viewMatrix;
	mat4 modelMatrix;
};

struct PostProcessResult {
	vec4 color;
	int mode;
};

#if POST_PROCESS_COMPUTE
vec4 Texture2DValue(image2D img, vec2 texCoord, vec2 resolution)
{
	return imageLoad(img, ivec2(texCoord * resolution));
}
#endif
#if !POST_PROCESS_COMPUTE
vec4 Texture2DValue(sampler2D img, vec2 texCoord, vec2 _resolution)
{
	return texture2D(img, texCoord);
}
#endif