#version 330 core
in vec4 FragPos;

uniform vec3 u_lightPos;
uniform float u_far;

void main()
{
    // get distance between fragment and light source
    float lightDistance = length(FragPos.xyz - u_lightPos);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / u_far;
    
    // write this as modified depth
    gl_FragColor = vec4(vec3(lightDistance), 1.0);
}  