#include "glfw_engine.h"
#include "game.h"
#include "input_manager.h"
#include "math/math_util.h"

#include <iostream>
#include <chrono>

#define USE_CHRONO 1

namespace apex {

static apex::InputManager *inputmgr;

static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    } else if (action == GLFW_PRESS) {
        inputmgr->KeyDown(key);
    } else if (action == GLFW_RELEASE) {
        inputmgr->KeyUp(key);
    }
}

static void ErrorCallback(int error, const char *description)
{
    std::cout << "Error: " << description << "\n";
}

bool GlfwEngine::InitializeGame(Game *game)
{
    glfwSetErrorCallback(ErrorCallback);

    if (!glfwInit()) {
        return false;
    }

    window = glfwCreateWindow(game->GetWindow().width, game->GetWindow().height,
        game->GetWindow().title.c_str(), NULL, NULL);

    if (!window) {
        glfwTerminate();
        return false;
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        throw std::exception("error initializing glew");
    }

    glfwSwapInterval(1);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    game->Initialize();

    inputmgr = game->GetInputManager();

#if USE_CHRONO
    auto last = std::chrono::high_resolution_clock::now();
#else
    double last = 0.0;
#endif
    while (!glfwWindowShouldClose(window)) {
#if USE_CHRONO
        auto current = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(current - last).count();
#else
        double current = glfwGetTime();
        double delta = current_time - last;
#endif

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwGetWindowSize(window, &game->GetWindow().width, &game->GetWindow().height);

        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        game->GetInputManager()->MouseMove(
            MathUtil::Clamp<double>(mouse_x, 0, game->GetWindow().width),
            MathUtil::Clamp<double>(mouse_y, 0, game->GetWindow().height));

        game->Logic(delta);
        game->Render();

        glfwSwapBuffers(window);
        glfwPollEvents();

        last = current;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return true;
}

void GlfwEngine::Viewport(int x, int y, size_t width, size_t height)
{
    glViewport(x, y, width, height);
}

void GlfwEngine::Clear(int mask)
{
    glClear(mask);
}

void GlfwEngine::SetMousePosition(double x, double y)
{
    int window_x, window_y;
    glfwGetWindowPos(window, &window_x, &window_y);
    glfwSetCursorPos(window, x, y);
}

void GlfwEngine::Enable(int cap)
{
    glEnable(cap);
}

void GlfwEngine::Disable(int cap)
{
    glDisable(cap);
}

void GlfwEngine::DepthMask(bool mask)
{
    glDepthMask(mask);
}

void GlfwEngine::BlendFunc(int src, int dst)
{
    glBlendFunc(src, dst);
}

void GlfwEngine::GenBuffers(size_t count, unsigned int *buffers)
{
    glGenBuffers(count, buffers);
}

void GlfwEngine::DeleteBuffers(size_t count, unsigned int *buffers)
{
    glDeleteBuffers(count, buffers);
}

void GlfwEngine::BindBuffer(int target, unsigned int buffer)
{
    glBindBuffer(target, buffer);
}

void GlfwEngine::BufferData(int target, size_t size, const void *data, int usage)
{
    glBufferData(target, size, data, usage);
}

void GlfwEngine::EnableVertexAttribArray(unsigned int index)
{
    glEnableVertexAttribArray(index);
}

void GlfwEngine::DisableVertexAttribArray(unsigned int index)
{
    glDisableVertexAttribArray(index);
}

void GlfwEngine::VertexAttribPointer(unsigned int index, int size, int type, bool normalized, size_t stride, void *ptr)
{
    glVertexAttribPointer(index, size, type, normalized, stride, ptr);
}

void GlfwEngine::DrawElements(int mode, size_t count, int type, const void *indices)
{
    glDrawElements(mode, count, type, indices);
}

void GlfwEngine::GenTextures(size_t n, unsigned int *textures)
{
    glGenTextures(n, textures);
}

void GlfwEngine::DeleteTextures(size_t n, const unsigned int *textures)
{
    glDeleteTextures(n, textures);
}

void GlfwEngine::TexParameteri(int target, int pname, int param)
{
    glTexParameteri(target, pname, param);
}

void GlfwEngine::TexParameterf(int target, int pname, float param)
{
    glTexParameterf(target, pname, param);
}

void GlfwEngine::TexImage2D(int target, int level, int ifmt, size_t width, size_t height,
    int border, int fmt, int type, const void *data)
{
    glTexImage2D(target, level, ifmt, width, height, border, fmt, type, data);
}

void GlfwEngine::BindTexture(int target, unsigned int texture)
{
    glBindTexture(target, texture);
}

void GlfwEngine::ActiveTexture(int i)
{
    glActiveTexture(i);
}

void GlfwEngine::GenerateMipmap(int target)
{
    glGenerateMipmap(target);
}

void GlfwEngine::GenFramebuffers(size_t n, unsigned int *ids)
{
    glGenFramebuffers(n, ids);
}

void GlfwEngine::DeleteFramebuffers(size_t n, const unsigned int *ids)
{
    glDeleteFramebuffers(n, ids);
}

void GlfwEngine::BindFramebuffer(int target, unsigned int framebuffer)
{
    glBindFramebuffer(target, framebuffer);
}

void GlfwEngine::FramebufferTexture(int target, int attachment, unsigned int texture, int level)
{
    glFramebufferTexture(target, attachment, texture, level);
}

void GlfwEngine::DrawBuffers(size_t n, const unsigned int *bufs)
{
    glDrawBuffers(n, bufs);
}

unsigned int GlfwEngine::CheckFramebufferStatus(int target)
{
    return glCheckFramebufferStatus(target);
}

unsigned int GlfwEngine::CreateProgram()
{
    return glCreateProgram();
}

unsigned int GlfwEngine::CreateShader(int type)
{
    return glCreateShader(type);
}

void GlfwEngine::ShaderSource(unsigned int shader, size_t count, const char **str, const int *len)
{
    glShaderSource(shader, count, str, len);
}

void GlfwEngine::CompileShader(unsigned int shader)
{
    glCompileShader(shader);
}

void GlfwEngine::AttachShader(unsigned int program, unsigned int shader)
{
    glAttachShader(program, shader);
}

void GlfwEngine::GetShaderiv(unsigned int shader, int pname, int *params)
{
    glGetShaderiv(shader, pname, params);
}

void GlfwEngine::GetShaderInfoLog(unsigned int shader, int max, int *len, char *info)
{
    glGetShaderInfoLog(shader, max, len, info);
}

void GlfwEngine::BindAttribLocation(unsigned int program, unsigned int index, const char *name)
{
    glBindAttribLocation(program, index, name);
}

void GlfwEngine::LinkProgram(unsigned int program)
{
    glLinkProgram(program);
}

void GlfwEngine::ValidateProgram(unsigned int program)
{
    glValidateProgram(program);
}

void GlfwEngine::GetProgramiv(unsigned int program, int pname, int *params)
{
    glGetProgramiv(program, pname, params);
}

void GlfwEngine::GetProgramInfoLog(unsigned int program, int max, int *len, char *log)
{
    glGetProgramInfoLog(program, max, len, log);
}

void GlfwEngine::DeleteProgram(unsigned int program)
{
    glDeleteProgram(program);
}

void GlfwEngine::DeleteShader(unsigned int shader)
{
    glDeleteShader(shader);
}

void GlfwEngine::UseProgram(unsigned int program)
{
    glUseProgram(program);
}

int GlfwEngine::GetUniformLocation(unsigned int program, const char *name)
{
    return glGetUniformLocation(program, name);
}

void GlfwEngine::Uniform1f(int location, float v0)
{
    glUniform1f(location, v0);
}

void GlfwEngine::Uniform2f(int location, float v0, float v1)
{
    glUniform2f(location, v0, v1);
}

void GlfwEngine::Uniform3f(int location, float v0, float v1, float v2)
{
    glUniform3f(location, v0, v1, v2);
}

void GlfwEngine::Uniform4f(int location, float v0, float v1, float v2, float v3)
{
    glUniform4f(location, v0, v1, v2, v3);
}

void GlfwEngine::Uniform1i(int location, int v0)
{
    glUniform1i(location, v0);
}

void GlfwEngine::Uniform2i(int location, int v0, int v1)
{
    glUniform2i(location, v0, v1);
}

void GlfwEngine::Uniform3i(int location, int v0, int v1, int v2)
{
    glUniform3i(location, v0, v1, v2);
}

void GlfwEngine::Uniform4i(int location, int v0, int v1, int v2, int v3)
{
    glUniform4i(location, v0, v1, v2, v3);
}

void GlfwEngine::UniformMatrix4fv(int location, int count, bool transpose, const float *value)
{
    glUniformMatrix4fv(location, count, transpose, value);
}

} // namespace apex