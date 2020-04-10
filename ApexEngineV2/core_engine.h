#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include <stddef.h>

#define APEX_MULTITHREADING 1

namespace apex {

class Game;

class CoreEngine {
public:
    static CoreEngine *GetInstance();
    static void SetInstance(CoreEngine *ptr);

    /*enum GLEnums {
        DEPTH_BUFFER_BIT = 0x00000100,
        STENCIL_BUFFER_BIT = 0x00000400,
        COLOR_BUFFER_BIT = 0x00004000,

        ZERO = 0,
        ONE = 1,
        SRC_COLOR = 0x0300,
        ONE_MINUS_SRC_COLOR = 0x0301,
        SRC_ALPHA = 0x0302,
        ONE_MINUS_SRC_ALPHA = 0x0303,
        DST_ALPHA = 0x0304,
        ONE_MINUS_DST_ALPHA = 0x0305,
        DST_COLOR = 0x0306,
        ONE_MINUS_DST_COLOR = 0x0307,
        SRC_ALPHA_SATURATE = 0x0308,

        BLEND = 0x0BE2, 
        DITHER = 0x0BD0,
        STENCIL_TEST = 0x0B90,
        DEPTH_TEST = 0x0B71,

        BYTE = 0x1400,
        UNSIGNED_BYTE = 0x1401,
        SHORT = 0x1402,
        UNSIGNED_SHORT = 0x1403,
        INT = 0x1404,
        UNSIGNED_INT = 0x1405,
        FLOAT = 0x1406,
        FIXED = 0x140C,

        DEPTH_COMPONENT = 0x1902,
        ALPHA = 0x1906,
        RGB = 0x1907,
        RGBA = 0x1908,
        RGBA4 = 0x8056,
        RGB5_A1 = 0x8057,
        RGB565 = 0x8D62,
        RGB8 = 0x8051,
        RGBA8 = 0x8058,
        RGB10_A2 = 0x8059,
        DEPTH_COMPONENT16 = 0x81A5,
        DEPTH_COMPONENT24 = 0x81A6,
        DEPTH_COMPONENT32F = 0x8CAC,

        NEAREST = 0x2600,
        LINEAR = 0x2601,
        NEAREST_MIPMAP_NEAREST = 0x2700,
        LINEAR_MIPMAP_NEAREST = 0x2701,
        NEAREST_MIPMAP_LINEAR = 0x2702,
        LINEAR_MIPMAP_LINEAR = 0x2703,
        TEXTURE_MAG_FILTER = 0x2800,
        TEXTURE_MIN_FILTER = 0x2801,
        TEXTURE_WRAP_S = 0x2802,
        TEXTURE_WRAP_T = 0x2803,
        TEXTURE = 0x1702,
        TEXTURE_CUBE_MAP = 0x8513,
        TEXTURE_BINDING_CUBE_MAP = 0x8514,
        TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515,
        TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516,
        TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517,
        TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518,
        TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519,
        TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A,
        MAX_CUBE_MAP_TEXTURE_SIZE = 0x851C,

        TEXTURE0 = 0x84C0,
        ACTIVE_TEXTURE = 0x84E0,
        REPEAT = 0x2901,
        CLAMP_TO_EDGE = 0x812F,
        MIRRORED_REPEAT = 0x8370,

        ARRAY_BUFFER = 0x8892,
        ELEMENT_ARRAY_BUFFER = 0x8893,

        STREAM_DRAW = 0x88E0,
        STATIC_DRAW = 0x88E4,
        DYNAMIC_DRAW = 0x88E8,
        FRONT = 0x0404,
        BACK = 0x0405,
        FRONT_AND_BACK = 0x0408,
        TEXTURE_2D = 0x0DE1,
        CULL_FACE = 0x0B44,

        FRAGMENT_SHADER = 0x8B30,
        VERTEX_SHADER = 0x8B31,

        COMPILE_STATUS = 0x8B81,
        LINK_STATUS = 0x8B82,
        VALIDATE_STATUS = 0x8B83,
        INFO_LOG_LENGTH = 0x8B84,

        FRAMEBUFFER = 0x8D40,
        RENDERBUFFER = 0x8D41,
        COLOR_ATTACHMENT0 = 0x8CE0,
        DEPTH_ATTACHMENT = 0x8D00,
        STENCIL_ATTACHMENT = 0x8D20,
        NONE = 0,
        FRAMEBUFFER_COMPLETE = 0x8CD5,
    };*/

    virtual bool InitializeGame(Game *game) = 0;
    virtual void Viewport(int x, int y, size_t width, size_t height) = 0;
    virtual void Clear(int mask) = 0;
    virtual void SetMousePosition(double x, double y) = 0;
    virtual void Enable(int cap) = 0;
    virtual void Disable(int cap) = 0;
    virtual void DepthMask(bool mask) = 0;
    virtual void BlendFunc(int src, int dst) = 0;
    virtual void GenBuffers(size_t count, unsigned int *buffers) = 0;
    virtual void DeleteBuffers(size_t count, unsigned int *buffers) = 0;
    virtual void BindBuffer(int target, unsigned int buffer) = 0;
    virtual void BufferData(int target, size_t size, const void *data, int usage) = 0;
    virtual void EnableVertexAttribArray(unsigned int index) = 0;
    virtual void DisableVertexAttribArray(unsigned int index) = 0;
    virtual void VertexAttribPointer(unsigned int index, int size, int type, bool normalized, size_t stride, void *ptr) = 0;
    virtual void DrawElements(int mode, size_t count, int type, const void *indices) = 0;
    virtual void GenTextures(size_t n, unsigned int *textures) = 0;
    virtual void DeleteTextures(size_t n, const unsigned int *textures) = 0;
    virtual void TexParameteri(int target, int pname, int param) = 0;
    virtual void TexParameterf(int target, int pname, float param) = 0;
    virtual void TexImage2D(int target, int level, int ifmt, size_t width, size_t height,
        int border, int fmt, int type, const void *data) = 0;
    virtual void BindTexture(int target, unsigned int texture) = 0;
    virtual void ActiveTexture(int i) = 0;
    virtual void GenerateMipmap(int target) = 0;
    virtual void GenFramebuffers(size_t n, unsigned int *ids) = 0;
    virtual void DeleteFramebuffers(size_t n, const unsigned int *ids) = 0;
    virtual void BindFramebuffer(int target, unsigned int framebuffer) = 0;
    virtual void FramebufferTexture(int target, int attachment, unsigned int texture, int level) = 0;
    virtual void DrawBuffers(size_t n, const unsigned int *bufs) = 0;
    virtual unsigned int CheckFramebufferStatus(int target) = 0;
    virtual unsigned int CreateProgram() = 0;
    virtual unsigned int CreateShader(int type) = 0;
    virtual void ShaderSource(unsigned int shader, size_t count, const char **str, const int *len) = 0;
    virtual void CompileShader(unsigned int shader) = 0;
    virtual void AttachShader(unsigned int program, unsigned int shader) = 0;
    virtual void GetShaderiv(unsigned int shader, int pname, int *params) = 0;
    virtual void GetShaderInfoLog(unsigned int shader, int max, int *len, char *info) = 0;
    virtual void BindAttribLocation(unsigned int program, unsigned int index, const char *name) = 0;
    virtual void LinkProgram(unsigned int program) = 0;
    virtual void ValidateProgram(unsigned int program) = 0;
    virtual void GetProgramiv(unsigned int program, int pname, int *params) = 0;
    virtual void GetProgramInfoLog(unsigned int program, int max, int *len, char *log) = 0;
    virtual void DeleteProgram(unsigned int program) = 0;
    virtual void DeleteShader(unsigned int shader) = 0;
    virtual void UseProgram(unsigned int program) = 0;
    virtual int GetUniformLocation(unsigned int program, const char *name) = 0;
    virtual void Uniform1f(int location, float v0) = 0;
    virtual void Uniform2f(int location, float v0, float v1) = 0;
    virtual void Uniform3f(int location, float v0, float v1, float v2) = 0;
    virtual void Uniform4f(int location, float v0, float v1, float v2, float v3) = 0;
    virtual void Uniform1i(int location, int v0) = 0;
    virtual void Uniform2i(int location, int v0, int v1) = 0;
    virtual void Uniform3i(int location, int v0, int v1, int v2) = 0;
    virtual void Uniform4i(int location, int v0, int v1, int v2, int v3) = 0;
    virtual void UniformMatrix4fv(int location, int count, bool transpose, const float *value) = 0;

private:
    static CoreEngine *instance;
};

} // namespace apex

#endif
