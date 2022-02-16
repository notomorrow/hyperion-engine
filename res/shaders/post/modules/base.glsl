#define $POST_PROCESS_COMPUTE 0


struct PostProcessData {
	vec2 texCoord;
	vec2 resolution;
	
	mat4 projMatrix;
	mat4 viewMatrix;
	mat4 modelMatrix;
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