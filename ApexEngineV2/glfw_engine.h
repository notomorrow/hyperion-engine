#ifndef GLFW_ENGINE_H
#define GLFW_ENGINE_H

#include "core_engine.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace apex {
class GlfwEngine : public CoreEngine {
public:
    bool InitializeGame(Game *game);
    void Viewport(int x, int y, size_t width, size_t height);
    void Clear(int mask);
    void SetMousePosition(double x, double y);
    void Enable(int cap);
    void Disable(int cap);
    void DepthMask(bool mask);
    void BlendFunc(int src, int dst);
    void GenBuffers(size_t count, unsigned int *buffers);
    void DeleteBuffers(size_t count, unsigned int *buffers);
    void BindBuffer(int target, unsigned int buffer);
    void BufferData(int target, size_t size, const void *data, int usage);
    void EnableVertexAttribArray(unsigned int index);
    void DisableVertexAttribArray(unsigned int index);
    void VertexAttribPointer(unsigned int index, int size, int type, bool normalized, size_t stride, void *ptr);
    void DrawElements(int mode, size_t count, int type, const void *indices);
    void GenTextures(size_t n, unsigned int *textures);
    void DeleteTextures(size_t n, const unsigned int *textures);
    void TexParameteri(int target, int pname, int param);
    void TexParameterf(int target, int pname, float param);
    void TexImage2D(int target, int level, int ifmt, size_t width, size_t height,
        int border, int fmt, int type, const void *data);
    void BindTexture(int target, unsigned int texture);
    void ActiveTexture(int i);
    void GenerateMipmap(int target);
    void GenFramebuffers(size_t n, unsigned int *ids);
    void DeleteFramebuffers(size_t n, const unsigned int *ids);
    void BindFramebuffer(int target, unsigned int framebuffer);
    void FramebufferTexture(int target, int attachment, unsigned int texture, int level);
    void DrawBuffers(size_t n, const unsigned int *bufs);
    unsigned int CheckFramebufferStatus(int target);
    unsigned int CreateProgram();
    unsigned int CreateShader(int type);
    void ShaderSource(unsigned int shader, size_t count, const char **str, const int *len);
    void CompileShader(unsigned int shader);
    void AttachShader(unsigned int program, unsigned int shader);
    void GetShaderiv(unsigned int shader, int pname, int *params);
    void GetShaderInfoLog(unsigned int shader, int max, int *len, char *info);
    void BindAttribLocation(unsigned int program, unsigned int index, const char *name);
    void LinkProgram(unsigned int program);
    void ValidateProgram(unsigned int program);
    void GetProgramiv(unsigned int program, int pname, int *params);
    void GetProgramInfoLog(unsigned int program, int max, int *len, char *log);
    void DeleteProgram(unsigned int program);
    void DeleteShader(unsigned int shader);
    void UseProgram(unsigned int program);
    int GetUniformLocation(unsigned int program, const char *name);
    void Uniform1f(int location, float v0);
    void Uniform2f(int location, float v0, float v1);
    void Uniform3f(int location, float v0, float v1, float v2);
    void Uniform4f(int location, float v0, float v1, float v2, float v3);
    void Uniform1i(int location, int v0);
    void Uniform2i(int location, int v0, int v1);
    void Uniform3i(int location, int v0, int v1, int v2);
    void Uniform4i(int location, int v0, int v1, int v2, int v3);
    void UniformMatrix4fv(int location, int count, bool transpose, const float *value);

private:
    GLFWwindow *window;
};
}

#endif