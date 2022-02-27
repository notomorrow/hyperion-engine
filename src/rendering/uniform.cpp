#include "uniform.h"
#include "shader.h"

#include "../opengl.h"
#include "../gl_util.h"

namespace hyperion {

void Uniform::BindUniform(Shader *shader, const char *name, int &texture_index)
{
    if (type == Uniform::UniformType::UNIFORM_TYPE_NONE) {
        // not set
        return;
    }

    int loc = glGetUniformLocation(shader->GetId(), name);

    if (loc != -1) {
        switch (type) {
        case Uniform::UniformType::UNIFORM_TYPE_FLOAT:
            glUniform1f(loc, data.f);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_I32:
            glUniform1i(loc, data.i32);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_I64:
            glUniform1i64ARB(loc, data.i64);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_U32:
            glUniform1ui(loc, data.u32);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_U64:
            glUniform1ui64ARB(loc, data.u64);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_VEC2:
            glUniform2f(loc, data.vec2[0], data.vec2[1]);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_VEC3:
            glUniform3f(loc, data.vec3[0], data.vec3[1], data.vec3[2]);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_VEC4:
            glUniform4f(loc, data.vec4[0], data.vec4[1], data.vec4[2], data.vec4[3]);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_MAT4:
            glUniformMatrix4fv(loc, 1, true, &data.mat4[0]);
            break;
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE2D:
            Texture::ActiveTexture(texture_index);
            glBindTexture(GL_TEXTURE_2D, data.texture_id);
            glUniform1i(loc, texture_index);
            texture_index++;
            break;
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURE3D:
            Texture::ActiveTexture(texture_index);
            glBindTexture(GL_TEXTURE_3D, data.texture_id);
            glUniform1i(loc, texture_index);
            texture_index++;
            break;
        case Uniform::UniformType::UNIFORM_TYPE_TEXTURECUBE:
            Texture::ActiveTexture(texture_index);
            glBindTexture(GL_TEXTURE_CUBE_MAP, data.texture_id);
            glUniform1i(loc, texture_index);
            texture_index++;
            break;
        default:
            std::cout << "invalid uniform type " << int(type) << " for " << name << "\n";
            break;
        }

        CatchGLErrors("Failed to set uniform", false);
    }
}

} // namespace hyperion