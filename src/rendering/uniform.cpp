#include "uniform.h"
#include "shader.h"

#include "../opengl.h"
#include "../gl_util.h"

namespace hyperion {

void Uniform::BindUniform(Shader *shader, const char *name, int &texture_index)
{
    if (type == Uniform::Uniform_None) {
        // not set
        return;
    }

    int loc = glGetUniformLocation(shader->GetId(), name);

    if (loc != -1) {
        switch (type) {
        case Uniform::Uniform_Float:
            glUniform1f(loc, data[0]);
            break;
        case Uniform::Uniform_Int:
            glUniform1i(loc, int(data[0]));
            break;
        case Uniform::Uniform_Vector2:
            glUniform2f(loc, data[0], data[1]);
            break;
        case Uniform::Uniform_Vector3:
            glUniform3f(loc, data[0], data[1], data[2]);
            break;
        case Uniform::Uniform_Vector4:
            glUniform4f(loc, data[0], data[1], data[2], data[3]);
            break;
        case Uniform::Uniform_Matrix4:
            glUniformMatrix4fv(loc, 1, true, &data[0]);
            break;
        case Uniform::Uniform_Texture2D:
            Texture::ActiveTexture(texture_index);
            glBindTexture(GL_TEXTURE_2D, int(data[0]));
            glUniform1i(loc, texture_index);
            texture_index++;
            break;
        case Uniform::Uniform_Texture3D:
            Texture::ActiveTexture(texture_index);
            glBindTexture(GL_TEXTURE_3D, int(data[0]));
            glUniform1i(loc, texture_index);
            texture_index++;
            break;
        case Uniform::Uniform_TextureCube:
            Texture::ActiveTexture(texture_index);
            glBindTexture(GL_TEXTURE_CUBE_MAP, int(data[0]));
            glUniform1i(loc, texture_index);
            texture_index++;
            break;
        default:
            std::cout << "invalid uniform type " << type << " for " << name << "\n";
            break;
        }

        CatchGLErrors("Failed to set uniform", false);
    }
}

} // namespace hyperion