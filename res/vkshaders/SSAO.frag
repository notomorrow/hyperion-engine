#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

// TODO: read in from prev pass.

layout(location=0) out vec4 color_output;

#include "include/gbuffer.inc"
#include "include/scene.inc"
#include "include/noise.inc"
#include "include/shared.inc"

vec2 texcoord = v_texcoord0;

#define SSAO_NOISE_AMOUNT 0.0002
vec2 GetNoise(vec2 coord) //generating noise/pattern texture for dithering
{
    float width = float(scene.resolution_x);
    float height = float(scene.resolution_y);
    
    float noise_x = ((fract(1.0 - coord.s * (width / 2.0)) * 0.25) + (fract(coord.t * (height / 2.0)) * 0.75)) * 2.0 - 1.0;
    float noise_y = ((fract(1.0 - coord.s * (width / 2.0)) * 0.75) + (fract(coord.t * (height / 2.0)) * 0.25)) * 2.0 - 1.0;

#if SSAO_NOISE
    noise_x = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453), 0.0, 1.0) * 2.0 - 1.0;
    noise_y = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233) * 2.0)) * 43758.5453), 0.0, 1.0) * 2.0 - 1.0;
#endif

    return vec2(noise_x, noise_y) * SSAO_NOISE_AMOUNT;
}

#define HYP_USE_GTAO

#ifndef HYP_USE_GTAO

const float diffarea = 0.3; //self-shadowing reduction
const float gdisplace = 0.4; //gauss bell center //0.4


#define SSAO_NOISE 1
#define SSAO_MIST 0
#define SSAO_MIST_START 0.0
#define SSAO_MIST_END 0.01
#define CAP_MIN_DISTANCE 0.0001
#define CAP_MAX_DISTANCE 0.1
#define SSAO_SAMPLES 25 // NOTE: Even numbers breaking on linux nvidia drivers ??
#define SSAO_STRENGTH 1.88
#define SSAO_CLAMP_AMOUNT 0.125
#define SSAO_RADIUS 2.0

#if SSAO_MIST
float CalculateMist()
{
    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    float depth_mist = -CAP_MAX_DISTANCE * CAP_MIN_DISTANCE / (depth * (CAP_MAX_DISTANCE - CAP_MIN_DISTANCE) - CAP_MAX_DISTANCE);
    
    return clamp((depth_mist - SSAO_MIST_START) / SSAO_MIST_END, 0.0, 1.0);
}
#endif

float ReadDepth(vec2 coord)
{
    float depth = SampleGBuffer(gbuffer_depth_texture, coord).r;
    
    if (coord.x < 0.0 || coord.y < 0.0) {
        return 1.0;
    }
    
    float z_n = 2.0 * depth - 1.0;
        
    return (2.0 * CAP_MIN_DISTANCE) / (CAP_MAX_DISTANCE + CAP_MIN_DISTANCE - z_n * (CAP_MAX_DISTANCE - CAP_MIN_DISTANCE));
}

int CompareDepthsFar(float depth1, float depth2)
{
    float garea = 2.0; //gauss bell width
    float diff = (depth1 - depth2) * 100.0; //depth difference (0-100)
    //reduce left bell width to avoid self-shadowing
    return int(diff >= gdisplace);
}

float CompareDepths(float depth1, float depth2)
{
    float diff = (depth1 - depth2) * 100.0; //depth difference (0-100)
    //reduce left bell width to avoid self-shadowing
    float garea = (diff < gdisplace) ? diffarea : 2.0; // gauss bell width

    return pow(2.7182, -2.0 * (diff - gdisplace) * (diff - gdisplace) / (garea * garea));
}

float CalculateAO(float depth, float dw, float dh)
{
    float dd = (1.0-depth)*SSAO_RADIUS;

    float temp = 0.0;
    float temp2 = 0.0;
    float coordw = texcoord.x + dw*dd;
    float coordh = texcoord.y + dh*dd;
    float coordw2 = texcoord.x - dw*dd;
    float coordh2 = texcoord.y - dh*dd;

    vec2 coord = vec2(coordw, coordh);
    vec2 coord2 = vec2(coordw2, coordh2);

    float cd = ReadDepth(coord);
    int far = CompareDepthsFar(depth, cd);
    temp = CompareDepths(depth, cd);
    //DEPTH EXTRAPOLATION:
    if (far > 0) {
        temp2 = CompareDepths(ReadDepth(coord2), depth);
        temp += (1.0 - temp) * temp2;
    }

    return temp;
}



void main()
{
    float width = float(scene.resolution_x);
    float height = float(scene.resolution_y);

    vec2 noise = GetNoise(texcoord);
    float depth = ReadDepth(texcoord);

    float w = (1.0 / width) / clamp(depth, SSAO_CLAMP_AMOUNT, 1.0) + (noise.x * (1.0 - noise.x));
    float h = (1.0 / height) / clamp(depth, SSAO_CLAMP_AMOUNT, 1.0) + (noise.y * (1.0 - noise.y));

    float pw = 0.0;
    float ph = 0.0;

    float ao = 0.0;

    float dl = HYP_FMATH_PI * (3.0 - sqrt(5.0));
    float dz = 1.0 / float(SSAO_SAMPLES);
    float l = 0.0;
    float z = 1.0 - dz/2.0;

    for (int i = 0; i < SSAO_SAMPLES; i++) {
        float r = sqrt(1.0 - z);

        pw = cos(l) * r;
        ph = sin(l) * r;
        ao += CalculateAO(depth, pw * w, ph * h);
        z = z - dz;
        l = l + dl;
    }

    ao /= float(SSAO_SAMPLES);
    ao *= SSAO_STRENGTH;
    ao = 1.0-ao;

#if SSAO_MIST
    ao = mix(ao, 1.0, CalculateMist());
#endif
    
    color_output = vec4(ao);
}


#else

#define HYP_GTAO_NUM_CIRCLES 4
#define HYP_GTAO_NUM_SLICES 8
#define HYP_GTAO_RADIUS 3.0
#define HYP_GTAO_THICKNESS 1.0
#define HYP_GTAO_POWER 1.0

#define HYP_GTAO_IRRADIANCE 1
#define HYP_GTAO_IRRADIANCE_NUM_SAMPLES 64

const float spatial_offsets[] = { 0.0, 0.5, 0.25, 0.75 };
const float temporal_rotations[] = { 60, 300, 180, 240, 120, 0 };

float fov_rad = HYP_FMATH_DEG2RAD(scene.camera_fov);

float tan_half_fov = tan(fov_rad * 0.5);
float inv_tan_half_fov = 1.0 / tan_half_fov;
vec2 focal_len = vec2(inv_tan_half_fov * (float(scene.resolution_y) / float(scene.resolution_x)), inv_tan_half_fov);
vec2 inv_focal_len = vec2(1.0) / focal_len;
vec4 uv_to_view = vec4(2.0 * inv_focal_len.x, 2.0 * inv_focal_len.y, -1.0 * inv_focal_len.x, -1.0 * inv_focal_len.y);

float GTAOOffsets(vec2 uv)
{
    ivec2 position = ivec2(uv * vec2(scene.resolution_x, scene.resolution_y));
    return 0.25 * float((position.y - position.x) & 3);
}

mat4 inv_view_proj = inverse(scene.projection * scene.view);
mat4 inv_proj = inverse(scene.projection);

vec3 GTAOGetPosition(vec2 uv, float depth)
{
    const float linear_depth = ViewDepth(depth, scene.camera_near, scene.camera_far); //LinearDepth(scene.projection, depth);//

    return vec3((vec2(uv.x, 1.0 - uv.y) * uv_to_view.xy + uv_to_view.zw) * linear_depth, linear_depth);

    // vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), uv, depth);
    // position = scene.view * position;
    // position /= position.w;
    // return position.xyz;
}

vec3 GTAOGetNormal(vec2 uv)
{
    vec3 normal = DecodeNormal(SampleGBuffer(gbuffer_normals_texture, uv));
    vec3 view_normal = (scene.view * vec4(normal, 0.0)).xyz;

    return view_normal;
}

float GTAOIntegrateUniformWeight(vec2 h)
{
    vec2 arc = vec2(1.0) - cos(h);

    return arc.x + arc.y;
}

float GTAOIntegrateArcCosWeight(vec2 h, float n)
{
    vec2 arc = -cos(2.0 * h - n) + cos(n) + 2.0 * h * sin(n);

    return 0.25 * (arc.x + arc.y);
}

vec2 GTAORotateDirection(vec2 uv, vec2 cos_sin)
{
    return vec2(
        uv.x * cos_sin.x - uv.y * cos_sin.y,
        uv.x * cos_sin.y + uv.y * cos_sin.x
    );
}

float CountImpact(float theta0, float theta1, float nx, float ny)
{
    float sin_theta0, cos_theta0, sin_theta1, cos_theta1;

    sin_theta0 = sin(theta0);
    cos_theta0 = cos(theta0);
    sin_theta1 = sin(theta1);
    cos_theta1 = cos(theta1);

    const float integral_x = (theta0 - sin_theta0 * cos_theta0) - (theta1 - sin_theta1 * cos_theta1);
    const float integral_y = cos_theta1 * cos_theta1 - cos_theta0 * cos_theta0;

    return Saturate(nx * integral_x + ny * integral_y);
}

void GTAOGetDetails(vec2 uv, out float occlusion, out vec3 bent_normal, out vec4 light_color)
{
    const float projected_scale = float(scene.resolution_y) / (tan_half_fov * 2.0) * 0.5;

    const float temporal_offset = spatial_offsets[(scene.frame_counter / 6) % 4];
    const float temporal_rotation = temporal_rotations[scene.frame_counter % 6];

    const float noise_offset = GTAOOffsets(vec2(uv.x, 1.0 - uv.y));
    const float noise_direction = InterleavedGradientNoise(vec2(uv.x, 1.0 - uv.y) * vec2(scene.resolution_x, scene.resolution_y));
    const float ray_step = fract(noise_offset + temporal_offset);

    const float depth = SampleGBuffer(gbuffer_depth_texture, uv).r;
    const vec3 P = GTAOGetPosition(uv, depth);
    const vec3 N = GTAOGetNormal(uv);
    const vec3 V = normalize(P);


    const vec2 texel_size = vec2(1.0) / vec2(scene.resolution_x, scene.resolution_y);

    const float camera_distance = length(V);

    const float step_radius = (projected_scale * 2.0 * HYP_GTAO_RADIUS) / max(camera_distance, 0.00001); //max(min((HYP_GTAO_RADIUS * projected_scale) / P.z, 512.0), float(HYP_GTAO_NUM_SLICES)) / float(HYP_GTAO_NUM_SLICES + 1);

    
    const float rotate_offset = rand(uv);



    bent_normal = vec3(0.0);
    occlusion = 0.0;
    light_color = vec4(0.0);

    for (int i = 0; i < HYP_GTAO_NUM_CIRCLES; i++) {
        const float angle = (float(i) + rotate_offset) / float(HYP_GTAO_NUM_CIRCLES) * 2.0 * HYP_FMATH_PI;
    
        vec2 ss_ray = { sin(angle), cos(angle) };

        const vec3 ray = normalize(vec3(ss_ray.xy * V.z, -dot(V.xy, ss_ray.xy)));

        ss_ray *= vec2(1.0, -1.0);

        const vec2 n = { dot(ray, N.xyz), -dot(N.xyz, V) };
        
        float cos_max_theta = -n.x * inversesqrt(HYP_FMATH_SQR(n.x) + max(n.y, 0.0) * n.y);
        float max_theta = acos(cos_max_theta);
        cos_max_theta = cos(max_theta);

        const vec2 rotated_position = {
            cos(float(i) + 1.0) * uv.x - sin(float(i) + 1.0) * uv.y,
            sin(float(i) + 1.0) * uv.x + cos(float(i) + 1.0) * uv.y
        };

        const float jitter_offset = rand(rotated_position);

        


        // vec2 horizons = vec2(-1.0);

        // const vec3 plane_normal = normalize(cross(slice_dir, V));
        // const vec3 plane_tangent = cross(V, plane_normal);
        // const vec3 projected_normal = N - plane_normal * dot(N, plane_normal);
        // const float projected_length = length(projected_normal);

        // const float cos_n = clamp(dot(normalize(projected_normal), V), -1.0, 1.0);
        // const float n = -sign(dot(projected_normal, plane_tangent)) * acos(cos_n);

        float slice_ao = 0.0;

        for (int j = 0; j < HYP_GTAO_NUM_SLICES; j++) {

            // const float iF = (float(i) + jitter_offset) / float(HYP_GTAO_NUM_SLICES);
            // const vec2 new_uv = (vec2(iF * step_radius) * ss_ray) + uv;

            const float step_base_noise = float(i + j * HYP_GTAO_NUM_SLICES) * 0.6180339887498948482;
            const float step_noise = fract(noise_direction + step_base_noise);
        
            // const vec2 uv_offset = (ss_ray * texel_size) * max(step_radius * (float(j) + ray_step), float(j + 1)) * vec2(1.0, -1.0);
            const vec2 new_uv = uv + (ss_ray * step_noise);

            const float new_depth = SampleGBuffer(gbuffer_depth_texture, new_uv).r;

            if (!all(equal(new_uv, uv)) && all(equal(new_uv, Saturate(new_uv)))) {
                const vec3 new_pos = GTAOGetPosition(new_uv, new_depth);
                const vec3 new_color = Texture2DLod(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture, new_uv.xy, 0.0).rgb;
                const vec3 new_normal = GTAOGetNormal(new_uv);

                vec3 origin_to_sample = new_pos - P;
                vec2 real_origin_to_sample = { -dot(origin_to_sample, V), dot(origin_to_sample, ray) };
                const float r_len = length(real_origin_to_sample);
                const float len = length(origin_to_sample);
                const float dist = r_len / HYP_GTAO_RADIUS;

                const float cos_theta = real_origin_to_sample.x / r_len;
                origin_to_sample /= len;

                // if ((cos_theta >= cos_max_theta) && dist < 1.0) {
                    const float theta = acos(cos_theta);
                    const float falloff = (1.0 - dist * dist) * (r_len / len);

                    const float impact = CountImpact(min(max_theta, theta + (HYP_FMATH_PI / 9.0)), theta, n.x, n.y);
                    light_color += vec4(new_color, 1.0) * impact;// * falloff;

                    max_theta += (theta - max_theta) * falloff;
                    cos_max_theta = cos(max_theta);

                    slice_ao += (1.0 - dist * dist) * (dot(origin_to_sample, N.rgb) - slice_ao);
                // }
            }

            // // R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
            // const float step_base_noise = float(i + j * HYP_GTAO_NUM_SLICES) * 0.6180339887498948482;
            // const float step_noise = fract(noise_direction + step_base_noise);
        
            // const vec2 uv_offset = (slice_dir.xy * texel_size) * max(step_radius * (float(j) + ray_step), float(j + 1)) * vec2(1.0, -1.0);
            // const vec4 uv_slice = uv.xyxy + vec4(uv_offset, -uv_offset);

            // const vec3 ds = GTAOGetPosition(uv_slice.xy) - P;
            // const vec3 dt = GTAOGetPosition(uv_slice.zw) - P;

            // const vec2 dsdt = vec2(dot(ds, ds), dot(dt, dt));
            // const vec2 dsdt_length = inversesqrt(dsdt);

            // const vec2 falloff = Saturate(dsdt * (2.0 / max(HYP_FMATH_SQR(HYP_GTAO_RADIUS), 0.0001)));

            // const vec2 H = vec2(dot(ds, V), dot(dt, V)) * dsdt_length;
            
            // // horizons = max(horizons, mix(H, horizons, falloff));
            // horizons = mix(
            //     mix(H, horizons, HYP_GTAO_THICKNESS),
            //     mix(H, horizons, falloff),
            //     greaterThan(H, horizons)
            // );

            // local_light += Texture2DLod(HYP_SAMPLER_LINEAR, gbuffer_albedo_texture, uv_slice.xy, 0.0) * 0.5;
            // local_light += Texture2DLod(HYP_SAMPLER_LINEAR, gbuffer_albedo_texture, uv_slice.zw, 0.0) * 0.5;
        }

        occlusion += slice_ao;

        // horizons = acos(clamp(horizons, vec2(-1.0), vec2(1.0)));
        // horizons.x = n + max(-horizons.x - n, -HYP_FMATH_PI_HALF);
        // horizons.y = n + min(horizons.y - n, HYP_FMATH_PI_HALF);

        // const float bent_angle = (horizons.x + horizons.y) * 0.5;
        // const float arc_cos_weight = GTAOIntegrateArcCosWeight(horizons, n);

        // bent_normal += V * cos(bent_angle) - plane_tangent * sin(bent_angle);
        // occlusion += projected_length * arc_cos_weight;

        // light_color += local_light / float(HYP_GTAO_NUM_SLICES);// * (1.0 - Saturate(occlusion));
    }

    // bent_normal = normalize(normalize(bent_normal) - (V * 0.5));
    occlusion = 1.0 - Saturate(pow(occlusion / float(HYP_GTAO_NUM_CIRCLES), HYP_GTAO_POWER));
    light_color /= max(light_color.a, HYP_FMATH_EPSILON);
}

#if HYP_GTAO_IRRADIANCE

vec3 SampleIrradiance(
    vec3 ray_origin,
    vec3 ray_direction
)
{
    bool intersect = false;

    float num_iterations = 0.0;

    vec3 ray_step = 3.0 * normalize(ray_direction);
    vec3 marching_position = ray_origin;
    float depth_from_screen;
    float step_delta;

    vec2 hit_pixel;

	int i = 0;

    // hit_pixel = GetProjectedPosition(scene.projection, marching_position);
    // return Texture2D(HYP_SAMPLER_LINEAR, gbuffer_albedo_texture, hit_pixel).rgb;

	for (; i < HYP_GTAO_IRRADIANCE_NUM_SAMPLES; i++) {
        marching_position += ray_step;

        hit_pixel = GetProjectedPosition(scene.projection, marching_position);

        float depth = FetchDepth(hit_pixel);

        depth_from_screen = ReconstructViewSpacePositionFromDepth(inv_proj, hit_pixel, depth).z;
        step_delta = marching_position.z - depth_from_screen;

        intersect = step_delta > 0.0;

        // if (step_delta > 0.95) {
        //     return vec3(0.0);
        // }

        num_iterations += 1.0;

        if (intersect) {
            break;
        }
    }

	if (intersect) {
		// binary search
		for (; i < HYP_GTAO_IRRADIANCE_NUM_SAMPLES; i++) {
            ray_step *= 0.5;
            marching_position = marching_position - ray_step * sign(step_delta);

            hit_pixel = GetProjectedPosition(scene.projection, marching_position);
            depth_from_screen = abs(ReconstructViewSpacePositionFromDepth(inv_proj, hit_pixel, FetchDepth(hit_pixel)).z);
            step_delta = abs(marching_position.z) - depth_from_screen;

            if (abs(step_delta) < 0.9) {
                #if 0
                float roughness = SampleGBuffer(gbuffer_material_texture, texcoord).r;
                roughness = clamp(roughness, 0.001, 0.999);

                const float cone_angle = RoughnessToConeAngle(roughness);

                const float current_radius = length((hit_pixel - texcoord) * vec2(scene.resolution_x, scene.resolution_y)) * cone_angle;
                const float max_mip_level = 4.0;
                float mip_level = clamp(log2(current_radius), 0.0, max_mip_level);
                return Texture2DLod(HYP_SAMPLER_LINEAR, gbuffer_mip_chain, hit_pixel, mip_level).rgb;
                #else
                return Texture2D(HYP_SAMPLER_LINEAR, gbuffer_albedo_texture, hit_pixel).rgb;
                #endif

            }
        }
    }

    return vec3(0.0);
}

#endif

void main()
{
    // color_output = vec4((inverse(scene.view) * (vec4(GTAOGetDetails(texcoord).xyz, 0.0))).xyz * 0.5 + 0.5, 1.0);
    
    float occlusion;
    vec3 bent_normal;
    vec4 light_color;
    
    GTAOGetDetails(texcoord, occlusion, bent_normal, light_color);

    color_output = vec4(light_color.rgb, occlusion);

    // Fade ray hits that approach the screen edge
    /*vec2 hit_pixel_ndc = texcoord * 2.0 - 1.0;
    float max_dimension = min(1.0, max(abs(hit_pixel_ndc.x), abs(hit_pixel_ndc.y)));
    color_output.a = mix(1.0, color_output.a, 1.0 - max(0.0, max_dimension - 0.9) / (1.0 - 0.98));*/
}

#endif
