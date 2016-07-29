uniform sampler2D u_shadowMap;
uniform mat4 u_shadowMatrix;

float getShadow(vec3 coord) {
    float bias = 0.005;
    float depth = texture2D(u_shadowMap, coord.xy).r;
    return max(step(coord.z-bias, depth), 0.0);
}

vec3 getShadowCoord(vec3 pos) {
    vec4 shadowPos = u_shadowMatrix * vec4(pos, 1.0);
    shadowPos *= vec4(0.5);
    shadowPos += vec4(0.5);
    return shadowPos.xyz;
}