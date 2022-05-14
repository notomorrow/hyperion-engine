
layout(set = 1, binding = 0) uniform sampler2D gbuffer_textures[3];
layout(set = 1, binding = 1) uniform sampler2D gbuffer_depth_texture;

#define gbuffer_albedo_texture    (gbuffer_textures[0])
#define gbuffer_normals_texture   (gbuffer_textures[1])
#define gbuffer_positions_texture (gbuffer_textures[2])

/*layout(set = 1, binding = 0) uniform sampler2D gbuffer_albedo_texture;
layout(set = 1, binding = 1) uniform sampler2D gbuffer_normals_texture;
layout(set = 1, binding = 2) uniform sampler2D gbuffer_positions_texture;
layout(set = 1, binding = 3) uniform sampler2D gbuffer_depth_texture;*/