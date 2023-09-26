#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec4 v_color;
layout(location=2) out vec4 v_conic_radius;
layout(location=3) out vec2 v_center_screen_position;
layout(location=4) out vec2 v_quad_position;
layout(location=5) out vec2 v_uv;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/object.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Gaussian.glsl"

layout(std430, set = 0, binding = 3) readonly buffer SplatIndicesBuffer
{
    uvec4 splat_indices[];
};

layout(std430, set = 0, binding = 4) readonly buffer SplatDistancesBuffer
{
    vec4 splat_distances[];
};

layout(std140, set = 0, binding = 5, row_major) readonly buffer SceneShaderData
{
    Scene scene;
};

/*layout(std140, set = 0, binding = 6, row_major) uniform CameraShaderData
{
    Camera camera;
};*/


void CalcCovariance3D(mat3 rotMat, out vec3 sigma0, out vec3 sigma1)
{
    mat3 sig = rotMat * transpose(rotMat);
    sigma0 = vec3(sig[0][0], sig[0][1], sig[0][2]);
    sigma1 = vec3(sig[1][1], sig[1][2], sig[2][2]);
}

vec3 CalcCovariance2D(vec3 worldPos, vec3 cov3d0, vec3 cov3d1, mat4 matrixV, mat4 matrixP, vec2 focal, vec2 tan_fov, vec4 screenParams)
{
    mat4 viewMatrix = matrixV;
    vec4 viewPos = viewMatrix * vec4(worldPos, 1.0);

    //float aspect = matrixP[0][0] / matrixP[1][1];
    //float tanFovX = 1.0 / matrixP[0][0];
    //float tanFovY = aspect / matrixP[1][1];
    float limX = 1.3 * tan_fov.x;
    float limY = 1.3 * tan_fov.y;
    viewPos.x = clamp(viewPos.x / viewPos.z, -limX, limX) * viewPos.z;
    viewPos.y = clamp(viewPos.y / viewPos.z, -limY, limY) * viewPos.z;

    //float focal = screenParams.x * matrixP[0][0] / 2.0;

    mat3 J = mat3(
        focal.x / viewPos.z, 0.0, -(focal.x * viewPos.x) / (viewPos.z * viewPos.z),
        0.0, focal.y / viewPos.z, -(focal.y * viewPos.y) / (viewPos.z * viewPos.z),
        0.0, 0.0, 0.0
    );
    /*viewMatrix[0][3] = 0.0;
    viewMatrix[1][3] = 0.0;
    viewMatrix[2][3] = 0.0;
    viewMatrix[3][3] = 1.0;*/

    mat3 W = mat3(transpose(viewMatrix));
    mat3 T = W * J;
    mat3 V = mat3(
        cov3d0.x, cov3d0.y, cov3d0.z,
        cov3d0.y, cov3d1.x, cov3d1.y,
        cov3d0.z, cov3d1.y, cov3d1.z
    );
    mat3 cov = transpose(T) * transpose(V) * T;

    // Low pass filter to make each splat at least 1px size.
    cov[0][0] += 0.3;
    cov[1][1] += 0.3;
    return vec3(cov[0][0], cov[0][1], cov[1][1]);
}

mat3 CalcMatrixFromRotationScale(vec4 rot, vec3 scale)
{
    mat3 ms = mat3(
        scale.x, 0.0, 0.0,
        0.0, scale.y, 0.0,
        0.0, 0.0, scale.z
    );
    float x = rot.w;
    float y = rot.x;
    float z = rot.y;
    float w = rot.z;
    mat3 mr = mat3(
        1.0-2.0*(y*y + z*z),   2.0*(x*y + w*z),   2.0*(x*z - w*y),
        2.0*(x*y - w*z), 1.0-2.0*(x*x + z*z),   2.0*(y*z + w*x),
        2.0*(x*z + w*y),   2.0*(y*z - w*x), 1.0-2.0*(x*x + y*y)
    );
    return mr*ms;
}

float rcp(float x)
{
    return 1.0 / x;
}

struct GaussianSplattingCamera {
    mat4 view;
    mat4 projection;
    vec2 focal;
    vec2 dimensions;
    float scale_modifier;
};

void main()
{
    GaussianSplattingCamera camera;
    camera.view = mat4(
        
        0.9620291590690613, 0.027075262740254402, -0.2716005742549896, 0.0,
        -0.014547135680913925, 0.9987397193908691, 0.0480351485311985, 0.0,
        0.2725588381290436, -0.04226019233465195, 0.9612105488777161, 0.0,
        -1.1606552600860596, -0.45992472767829895, 3.8893790245056152, 1.0
    );

    camera.projection = mat4(
        1.188818931579589, 0.0, 0.0, 0.0,
        0.0, 2.1333301067352295, 0.0, 0.0,
        0.0, 0.0, 1.0020040273666382, 1.0,
        0.0, 0.0, -0.20040079951286316, 0.0
    );

    camera.focal = vec2(581.3324518791126, 581.3324518791126);
    camera.dimensions = vec2(700.0, 700.0);
    camera.scale_modifier = 1.2844036697247707;
    vec2 tan_half_fov = vec2(camera.dimensions / camera.focal) * 0.5;


    const int instance_id = gl_InstanceIndex;
    const uint sorted_index = splat_indices[instance_id >> 2][instance_id & 3];

    const vec2 quad_position = a_position.xy;

    GaussianSplatShaderData instance = instances[sorted_index];

    vec4 rotation = instance.rotation;
    vec3 scale = instance.scale.xyz * camera.scale_modifier;

    mat3 rotation_scale_matrix = CalcMatrixFromRotationScale(rotation, scale);

    vec3 covariance_3d_0;
    vec3 covariance_3d_1;
    CalcCovariance3D(rotation_scale_matrix, covariance_3d_0, covariance_3d_1);
    vec3 covariance_2d = CalcCovariance2D(
        instance.position.xyz,
        covariance_3d_0,
        covariance_3d_1,
        camera.view,
        camera.projection,
        camera.focal,
        tan_half_fov,
        vec4(camera.dimensions.xy, 0.0, 0.0)
    );

    float det = covariance_2d.x * covariance_2d.z - covariance_2d.y * covariance_2d.y;

    float mid = 0.5 * (covariance_2d.x + covariance_2d.z);
    float lambda1 = mid + sqrt(max(0.1, mid * mid - det));
    float lambda2 = mid - sqrt(max(0.1, mid * mid - det));
    float radius = ceil(3.0 * sqrt(max(lambda1, lambda2)));

    vec3 conic = vec3(covariance_2d.z, -covariance_2d.y, covariance_2d.x) * rcp(det);
    const vec4 conic_radius = vec4(conic, radius);

    vec4 center_ndc_position = camera.projection * camera.view * vec4(instance.position.xyz, 1.0);
    center_ndc_position /= center_ndc_position.w;

    vec2 center_screen_position = (center_ndc_position.xy * 0.5 + 0.5) * camera.dimensions.xy;

    vec2 delta_screen_position = quad_position * radius * 2.0 / camera.dimensions.xy;
    
    gl_Position = center_ndc_position + vec4(delta_screen_position, 0.0, 0.0);

    v_color = instance.color; ;//vec4(vec3(splat_distances[instance_id >> 2][instance_id & 3], 0.0, 0.0), 1.0); ////
    v_position = gl_Position.xyz;
    v_quad_position = quad_position;
    v_conic_radius = conic_radius;
    v_uv = vec2(quad_position * radius);
} 
