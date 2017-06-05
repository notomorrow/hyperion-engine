#include "shader_code.h"

namespace apex {

const char *ShaderCode::aabb_debug_vs =
    "#version 330\n"
    "attribute vec3 a_position;"
    "varying vec4 v_position;"
    "uniform mat4 u_modelMatrix;"
    "uniform mat4 u_viewMatrix;"
    "uniform mat4 u_projMatrix;"
    "void main() {"
    "    v_position = vec4(a_position, 1.0);"
    "    gl_Position = u_projMatrix * u_viewMatrix * v_position;"
    "}";

const char *ShaderCode::aabb_debug_fs =
    "#version 330\n"
    "varying vec4 v_position;"
    "void main() {"
    "    gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);"
    "}";

} // namespace apex