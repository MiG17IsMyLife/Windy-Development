#define WIN32_LEAN_AND_MEAN
// Ensure we get prototypes for GL extensions (glActiveTexture, etc)
#define GL_GLEXT_PROTOTYPES
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h> // Provides declarations for GL > 1.1
#include <GL/glu.h>

#include "glhooks.h"
// #include "shaderpatches.h"
#include "../../core/log.h"
#include <string.h>

// Cross-platform return address support
#ifdef _MSC_VER
#include <intrin.h>
#define GET_RETURN_ADDRESS() _ReturnAddress()
#else
#define GET_RETURN_ADDRESS() __builtin_return_address(0)
#endif
// #include <vector>

// Macros for efficient wrapper generation
// -----------------------------------------------------------------------------
// WRAP / WRAP_VOID Macros:
// These macros generate static functions (e.g., my_glBegin) that act as thunks.
// They load the real OpenGL function pointer on first use via GLXBridge::GetProcAddress.
// Note: These functions are NOT automatically exposed. You must manually register
// them in the GetProcAddress function below using the MAP macro.
// -----------------------------------------------------------------------------
static void *my_GetProcAddress(const char *name)
{
    // 1. Try the standard Windows Extension loader
    void *p = (void *)wglGetProcAddress(name);

    // 2. Check for those weird legacy return values NVIDIA/Windows might return
    if (p == NULL || p == (void *)0x1 || p == (void *)0x2 || p == (void *)0x3 || p == (void *)-1)
    {

        // 3. Fallback: Try the raw export from opengl32.dll (for GL 1.1 functions)
        static HMODULE lib = GetModuleHandleA("opengl32.dll");
        p = (void *)GetProcAddress(lib, name);
    }

    // 4. NVIDIA-Specific Bonus: If still NULL, try the ARB suffix fallback
    if (p == NULL)
    {
        char fallbackName[256];

        // Try ARB first
        snprintf(fallbackName, sizeof(fallbackName), "%sARB", name);
        p = (void *)wglGetProcAddress(fallbackName);

        // If no ARB, try EXT (Common for glSecondaryColor, glPointParameter, etc)
        if (p == NULL || p == (void *)0x1 || p == (void *)0x2 || p == (void *)0x3 || p == (void *)-1)
        {
            snprintf(fallbackName, sizeof(fallbackName), "%sEXT", name);
            p = (void *)wglGetProcAddress(fallbackName);
        }
    }

    return p;
}

#define WRAP(name, ret, args, params)                                                                                                      \
    static ret my_##name args                                                                                                              \
    {                                                                                                                                      \
        static auto real = (decltype(name) *)my_GetProcAddress(#name);                                                                     \
        if (!real)                                                                                                                         \
        {                                                                                                                                  \
            log_error("GLHooks: Missing " #name);                                                                                          \
            return (ret)0;                                                                                                                 \
        }                                                                                                                                  \
        /* log_trace(#name); */                                                                                                            \
        return real params;                                                                                                                \
    }

#define WRAP_VOID(name, args, params)                                                                                                      \
    static void my_##name args                                                                                                             \
    {                                                                                                                                      \
        static auto real = (decltype(name) *)my_GetProcAddress(#name);                                                                     \
        if (!real)                                                                                                                         \
        {                                                                                                                                  \
            log_error("GLHooks: Missing " #name);                                                                                          \
            return;                                                                                                                        \
        }                                                                                                                                  \
        /* log_trace(#name); */                                                                                                            \
        real params;                                                                                                                       \
    }

// --- Wrapper Implementations ---

WRAP_VOID(glBegin, (GLenum mode), (mode))
WRAP_VOID(glEnd, (), ())
WRAP_VOID(glVertex3f, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
WRAP_VOID(glColor4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), (r, g, b, a))
WRAP_VOID(glEnable, (GLenum cap), (cap))
WRAP_VOID(glDisable, (GLenum cap), (cap))
// Returns int to ensure register EAX is fully set (0 or 1),
// avoiding garbage in high bytes if caller expects 32-bit BOOL.
WRAP(glIsEnabled, int, (GLenum cap), (cap))

WRAP_VOID(glMatrixMode, (GLenum mode), (mode))
WRAP_VOID(glLoadIdentity, (), ())
WRAP_VOID(glPushMatrix, (), ())
WRAP_VOID(glPopMatrix, (), ())
WRAP_VOID(glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l, r, b, t, n, f))
WRAP_VOID(glTranslated, (GLdouble x, GLdouble y, GLdouble z), (x, y, z))
WRAP_VOID(glTranslatef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
WRAP_VOID(glRotatef, (GLfloat a, GLfloat x, GLfloat y, GLfloat z), (a, x, y, z))
WRAP_VOID(glScalef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))

WRAP_VOID(glDepthRange, (GLclampd n, GLclampd f), (n, f))
WRAP_VOID(glPolygonOffset, (GLfloat factor, GLfloat units), (factor, units))
WRAP_VOID(glFlush, (), ())
WRAP_VOID(glFinish, (), ())

WRAP_VOID(glClear, (GLbitfield mask), (mask))
WRAP_VOID(glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a), (r, g, b, a))

WRAP_VOID(glBindTexture, (GLenum target, GLuint texture), (target, texture))
WRAP_VOID(glDeleteTextures, (GLsizei n, const GLuint *textures), (n, textures))
WRAP_VOID(glGenTextures, (GLsizei n, GLuint *textures), (n, textures))
WRAP_VOID(glTexParameteri, (GLenum target, GLenum pname, GLint param), (target, pname, param))
WRAP_VOID(glTexImage2D,
          (GLenum target, GLint level, GLint intfmt, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type, const GLvoid *pixels),
          (target, level, intfmt, w, h, border, fmt, type, pixels))

// Missing functions found in crash report
WRAP_VOID(glGetLightfv, (GLenum light, GLenum pname, GLfloat *params), (light, pname, params))

// Basic State
WRAP_VOID(glCullFace, (GLenum mode), (mode))
WRAP_VOID(glFrontFace, (GLenum mode), (mode))
WRAP_VOID(glHint, (GLenum target, GLenum mode), (target, mode))
WRAP_VOID(glLineWidth, (GLfloat width), (width))
WRAP_VOID(glPointSize, (GLfloat size), (size))
WRAP_VOID(glPolygonMode, (GLenum face, GLenum mode), (face, mode))
WRAP_VOID(glScissor, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height))
WRAP_VOID(glShadeModel, (GLenum mode), (mode))

// Pixel Store
WRAP_VOID(glPixelStorei, (GLenum pname, GLint param), (pname, param))
WRAP_VOID(glPixelStoref, (GLenum pname, GLfloat param), (pname, param))

// State Retrieval
WRAP_VOID(glGetBooleanv, (GLenum pname, GLboolean *params), (pname, params))
WRAP_VOID(glGetDoublev, (GLenum pname, GLdouble *params), (pname, params))
WRAP_VOID(glGetFloatv, (GLenum pname, GLfloat *params), (pname, params))
WRAP_VOID(glGetIntegerv, (GLenum pname, GLint *params), (pname, params))
WRAP_VOID(glPushAttrib, (GLbitfield mask), (mask))
WRAP_VOID(glPopAttrib, (), ())
WRAP_VOID(glPushClientAttrib, (GLbitfield mask), (mask))
WRAP_VOID(glPopClientAttrib, (), ())

// Blending / Alpha / Depth
WRAP_VOID(glAlphaFunc, (GLenum func, GLclampf ref), (func, ref))
WRAP_VOID(glBlendFunc, (GLenum sfactor, GLenum dfactor), (sfactor, dfactor))
WRAP_VOID(glLogicOp, (GLenum opcode), (opcode))
WRAP_VOID(glStencilFunc, (GLenum func, GLint ref, GLuint mask), (func, ref, mask))
WRAP_VOID(glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass), (fail, zfail, zpass))
WRAP_VOID(glDepthFunc, (GLenum func), (func))
WRAP_VOID(glDepthMask, (GLboolean flag), (flag))

// Lighting
WRAP_VOID(glLightfv, (GLenum light, GLenum pname, const GLfloat *params), (light, pname, params))
WRAP_VOID(glLightf, (GLenum light, GLenum pname, GLfloat param), (light, pname, param))
WRAP_VOID(glLightModelfv, (GLenum pname, const GLfloat *params), (pname, params))
WRAP_VOID(glMaterialfv, (GLenum face, GLenum pname, const GLfloat *params), (face, pname, params))
WRAP_VOID(glNormal3f, (GLfloat nx, GLfloat ny, GLfloat nz), (nx, ny, nz))

// Textures Coords
WRAP_VOID(glTexCoord2f, (GLfloat s, GLfloat t), (s, t))
WRAP_VOID(glTexCoord2fv, (const GLfloat *v), (v))

// Multitexture
WRAP_VOID(glActiveTexture, (GLenum texture), (texture))
WRAP_VOID(glMultiTexCoord2f, (GLenum target, GLfloat s, GLfloat t), (target, s, t))
WRAP_VOID(glMultiTexCoord2fv, (GLenum target, const GLfloat *v), (target, v))
// ARB Variants (Common in older Linux binaries)
WRAP_VOID(glActiveTextureARB, (GLenum texture), (texture))
WRAP_VOID(glMultiTexCoord2fARB, (GLenum target, GLfloat s, GLfloat t), (target, s, t))
WRAP_VOID(glMultiTexCoord2fvARB, (GLenum target, const GLfloat *v), (target, v))

// Vertex Variants
WRAP_VOID(glVertex2d, (GLdouble x, GLdouble y), (x, y))
WRAP_VOID(glVertex2f, (GLfloat x, GLfloat y), (x, y))
WRAP_VOID(glVertex2i, (GLint x, GLint y), (x, y))
WRAP_VOID(glVertex3fv, (const GLfloat *v), (v))
WRAP_VOID(glRasterPos2i, (GLint x, GLint y), (x, y))
WRAP_VOID(glRasterPos3f, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))

// Color Variants
WRAP_VOID(glColor3f, (GLfloat r, GLfloat g, GLfloat b), (r, g, b))
WRAP_VOID(glColor4ub, (GLubyte r, GLubyte g, GLubyte b, GLubyte a), (r, g, b, a))
WRAP_VOID(glColor4fv, (const GLfloat *v), (v))

// Rect
WRAP_VOID(glRectf, (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2), (x1, y1, x2, y2))
WRAP_VOID(glRecti, (GLint x1, GLint y1, GLint x2, GLint y2), (x1, y1, x2, y2))

// Display Lists (Very common)
WRAP_VOID(glNewList, (GLuint list, GLenum mode), (list, mode))
WRAP_VOID(glEndList, (), ())
WRAP_VOID(glCallList, (GLuint list), (list))
WRAP(glGenLists, GLuint, (GLsizei range), (range))
WRAP_VOID(glDeleteLists, (GLuint list, GLsizei range), (list, range))
WRAP(glIsList, GLboolean, (GLuint list), (list))

// Strings & Errors
WRAP(glGetString, const GLubyte *, (GLenum name), (name))
WRAP(glGetError, GLenum, (), ())

// Camera
WRAP_VOID(glFrustum, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar),
          (left, right, bottom, top, zNear, zFar))

// Vertex Arrays / Client State
WRAP_VOID(glEnableClientState, (GLenum array), (array))
WRAP_VOID(glDisableClientState, (GLenum array), (array))
WRAP_VOID(glClientActiveTexture, (GLenum texture), (texture))
WRAP_VOID(glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer), (size, type, stride, pointer))
WRAP_VOID(glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer), (size, type, stride, pointer))
WRAP_VOID(glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *pointer), (type, stride, pointer))
WRAP_VOID(glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer), (size, type, stride, pointer))
WRAP_VOID(glDrawArrays, (GLenum mode, GLint first, GLsizei count), (mode, first, count))
WRAP_VOID(glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices), (mode, count, type, indices))

// Texture Env
WRAP_VOID(glTexEnvf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param))
WRAP_VOID(glTexEnvi, (GLenum target, GLenum pname, GLint param), (target, pname, param))
WRAP_VOID(glTexEnvfv, (GLenum target, GLenum pname, const GLfloat *params), (target, pname, params))
WRAP_VOID(glTexEnviv, (GLenum target, GLenum pname, const GLint *params), (target, pname, params))

// --- Requested Missing Functions (Batch 2) ---

// Color / Material / Fog / Light
WRAP_VOID(glColor3ub, (GLubyte red, GLubyte green, GLubyte blue), (red, green, blue))
WRAP_VOID(glColorMaterial, (GLenum face, GLenum mode), (face, mode))
WRAP_VOID(glFogf, (GLenum pname, GLfloat param), (pname, param))
WRAP_VOID(glFogfv, (GLenum pname, const GLfloat *params), (pname, params))
WRAP_VOID(glFogi, (GLenum pname, GLint param), (pname, param))
WRAP_VOID(glMaterialf, (GLenum face, GLenum pname, GLfloat param), (face, pname, param))
WRAP_VOID(glLightModeli, (GLenum pname, GLint param), (pname, param))
WRAP_VOID(glSecondaryColor3ub, (GLubyte red, GLubyte green, GLubyte blue), (red, green, blue))
WRAP_VOID(glBlendColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha))
WRAP_VOID(glBlendEquation, (GLenum mode), (mode))

// Textures / Points
WRAP_VOID(glTexCoord4f, (GLfloat s, GLfloat t, GLfloat r, GLfloat q), (s, t, r, q))
WRAP_VOID(glCompressedTexImage2DARB,
          (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
           const GLvoid *data),
          (target, level, internalformat, width, height, border, imageSize, data))
WRAP_VOID(glTexParameterfv, (GLenum target, GLenum pname, const GLfloat *params), (target, pname, params))
WRAP_VOID(glPointParameterf, (GLenum pname, GLfloat param), (pname, param))
WRAP_VOID(glPointParameterfv, (GLenum pname, const GLfloat *params), (pname, params))

// Vertex / Matrix
WRAP_VOID(glVertex4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), (x, y, z, w))
WRAP_VOID(glMultMatrixf, (const GLfloat *m), (m))

// Extensions: Framebuffers (EXT)
WRAP_VOID(glGenFramebuffersEXT, (GLsizei n, GLuint *framebuffers), (n, framebuffers))
WRAP_VOID(glBindFramebufferEXT, (GLenum target, GLuint framebuffer), (target, framebuffer))
WRAP_VOID(glFramebufferTexture2DEXT, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level),
          (target, attachment, textarget, texture, level))
WRAP_VOID(glDeleteFramebuffersEXT, (GLsizei n, const GLuint *framebuffers), (n, framebuffers))

// Extensions: Vertex Programs / Shaders (ARB)
WRAP(glIsProgramARB, GLboolean, (GLuint program), (program))
WRAP_VOID(glGenProgramsARB, (GLsizei n, GLuint *programs), (n, programs))
WRAP_VOID(glBindProgramARB, (GLenum target, GLuint program), (target, program))
WRAP_VOID(glProgramStringARB, (GLenum target, GLenum format, GLsizei len, const GLvoid *string), (target, format, len, string))
WRAP_VOID(glProgramLocalParameter4fARB, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
          (target, index, x, y, z, w))
WRAP_VOID(glProgramLocalParameter4fvARB, (GLenum target, GLuint index, const GLfloat *params), (target, index, params))
WRAP_VOID(glGetProgramivARB, (GLenum target, GLenum pname, GLint *params), (target, pname, params))

// Extensions: Vertex Arrays (ARB/EXT)
WRAP_VOID(glClientActiveTextureARB, (GLenum texture), (texture))
WRAP_VOID(glEnableVertexAttribArrayARB, (GLuint index), (index))
WRAP_VOID(glDisableVertexAttribArrayARB, (GLuint index), (index))
WRAP_VOID(glVertexAttribPointerARB, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer),
          (index, size, type, normalized, stride, pointer))

// Buffers
WRAP_VOID(glBindBuffer, (GLenum target, GLuint buffer), (target, buffer))
WRAP_VOID(glDeleteBuffers, (GLsizei n, const GLuint *buffers), (n, buffers))
WRAP_VOID(glGenBuffers, (GLsizei n, GLuint *buffers), (n, buffers))
WRAP_VOID(glBufferData, (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage), (target, size, data, usage))

// Framebuffers
WRAP_VOID(glGenFramebuffers, (GLsizei n, GLuint *framebuffers), (n, framebuffers))
WRAP_VOID(glBindFramebuffer, (GLenum target, GLuint framebuffer), (target, framebuffer))
WRAP_VOID(glFramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level),
          (target, attachment, textarget, texture, level))
WRAP(glCheckFramebufferStatus, GLenum, (GLenum target), (target))
WRAP_VOID(glDeleteFramebuffers, (GLsizei n, const GLuint *framebuffers), (n, framebuffers))
WRAP_VOID(glFramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer),
          (target, attachment, renderbuffertarget, renderbuffer))
WRAP_VOID(glGenRenderbuffers, (GLsizei n, GLuint *renderbuffers), (n, renderbuffers))
WRAP_VOID(glBindRenderbuffer, (GLenum target, GLuint renderbuffer), (target, renderbuffer))
WRAP_VOID(glRenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height),
          (target, internalformat, width, height))
WRAP_VOID(glDeleteRenderbuffers, (GLsizei n, const GLuint *renderbuffers), (n, renderbuffers))
WRAP_VOID(glDrawBuffers, (GLsizei n, const GLenum *bufs), (n, bufs))
WRAP_VOID(glPixelMapusv, (GLenum map, GLsizei size, const GLushort *values), (map, size, values))
WRAP_VOID(glPixelTransferf, (GLenum pname, GLfloat param), (pname, param))
WRAP_VOID(glPixelTransferi, (GLenum pname, GLint param), (pname, param))
WRAP_VOID(glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels),
          (x, y, width, height, format, type, pixels))

// Shaders / Programs
WRAP(glCreateShader, GLuint, (GLenum type), (type))
WRAP_VOID(glShaderSource, (GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length), (shader, count, string, length))
WRAP_VOID(glCompileShader, (GLuint shader), (shader))
WRAP_VOID(glGetShaderiv, (GLuint shader, GLenum pname, GLint *params), (shader, pname, params))
WRAP_VOID(glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog), (shader, bufSize, length, infoLog))
WRAP_VOID(glDeleteShader, (GLuint shader), (shader))

WRAP(glCreateProgram, GLuint, (), ())
WRAP_VOID(glAttachShader, (GLuint program, GLuint shader), (program, shader))
WRAP_VOID(glLinkProgram, (GLuint program), (program))
WRAP_VOID(glGetProgramiv, (GLuint program, GLenum pname, GLint *params), (program, pname, params))
WRAP_VOID(glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog), (program, bufSize, length, infoLog))
WRAP_VOID(glUseProgram, (GLuint program), (program))
WRAP_VOID(glDeleteProgram, (GLuint program), (program))

// Uniforms / Attributes
WRAP(glGetUniformLocation, GLint, (GLuint program, const GLchar *name), (program, name))
WRAP(glGetAttribLocation, GLint, (GLuint program, const GLchar *name), (program, name))
WRAP_VOID(glUniform1f, (GLint location, GLfloat v0), (location, v0))
WRAP_VOID(glUniform1fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))
WRAP_VOID(glUniform1i, (GLint location, GLint v0), (location, v0))
WRAP_VOID(glUniform1iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))
WRAP_VOID(glUniform2fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))
WRAP_VOID(glUniform2iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))
WRAP_VOID(glUniform3fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))
WRAP_VOID(glUniform3iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))
WRAP_VOID(glUniform4fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))
WRAP_VOID(glUniform4iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))
WRAP_VOID(glUniformMatrix2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value),
          (location, count, transpose, value))
WRAP_VOID(glUniformMatrix3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value),
          (location, count, transpose, value))
WRAP_VOID(glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value),
          (location, count, transpose, value))
WRAP_VOID(glGetActiveUniform, (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name),
          (program, index, bufSize, length, size, type, name))

WRAP_VOID(glEnableVertexAttribArray, (GLuint index), (index))
WRAP_VOID(glDisableVertexAttribArray, (GLuint index), (index))
WRAP_VOID(glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer),
          (index, size, type, normalized, stride, pointer))

// Textures
WRAP_VOID(glTexParameterf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param))
WRAP_VOID(glTexSubImage2D,
          (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
           const GLvoid *pixels),
          (target, level, xoffset, yoffset, width, height, format, type, pixels))
WRAP_VOID(glInterleavedArrays, (GLenum format, GLsizei stride, const GLvoid *pointer), (format, stride, pointer))

// --- Requested Missing Functions (Batch 4) ---

WRAP_VOID(glSetFenceNV, (GLuint fence, GLenum condition), (fence, condition))
WRAP(glIsTexture, GLboolean, (GLuint texture), (texture))
WRAP_VOID(glMultTransposeMatrixf, (const GLfloat *m), (m))
WRAP_VOID(glColor4ubv, (const GLubyte *v), (v))
WRAP_VOID(glCompressedTexImage2D,
          (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
           const GLvoid *data),
          (target, level, internalformat, width, height, border, imageSize, data))
WRAP(glMapBuffer, GLvoid *, (GLenum target, GLenum access), (target, access))
WRAP_VOID(glFinishFenceNV, (GLuint fence), (fence))
WRAP_VOID(glVertexAttrib1f, (GLuint index, GLfloat x), (index, x))
WRAP_VOID(glClampColorARB, (GLenum target, GLenum clamp), (target, clamp))
WRAP_VOID(glBlendFuncSeparate, (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),
          (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))
WRAP_VOID(glGetBufferParameterivARB, (GLenum target, GLenum pname, GLint *params), (target, pname, params))
WRAP_VOID(glDeleteProgramsARB, (GLsizei n, const GLuint *programs), (n, programs))
WRAP_VOID(glGetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint *params), (target, level, pname, params))
WRAP_VOID(glRasterPos2f, (GLfloat x, GLfloat y), (x, y))
WRAP_VOID(glDeleteRenderbuffersEXT, (GLsizei n, const GLuint *renderbuffers), (n, renderbuffers))
WRAP_VOID(glDeleteBuffersARB, (GLsizei n, const GLuint *buffers), (n, buffers))
WRAP_VOID(glCopyTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height),
          (target, level, xoffset, yoffset, x, y, width, height))
WRAP_VOID(glBufferDataARB, (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage), (target, size, data, usage))
WRAP_VOID(glBindBufferARB, (GLenum target, GLuint buffer), (target, buffer))
WRAP(glIsBufferARB, GLboolean, (GLuint buffer), (buffer))
WRAP_VOID(glLoadTransposeMatrixf, (const GLfloat *m), (m))
WRAP_VOID(glGetMaterialfv, (GLenum face, GLenum pname, GLfloat *params), (face, pname, params))
WRAP(glUnmapBuffer, GLboolean, (GLenum target), (target))
WRAP_VOID(glProgramEnvParameter4fARB, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
          (target, index, x, y, z, w))
WRAP_VOID(glReadBuffer, (GLenum mode), (mode))
WRAP(glIsFenceNV, GLboolean, (GLuint fence), (fence))
WRAP_VOID(glProgramEnvParameter4fvARB, (GLenum target, GLuint index, const GLfloat *params), (target, index, params))

// Misc
WRAP_VOID(glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha), (red, green, blue, alpha))
WRAP_VOID(glClearDepth, (GLclampd depth), (depth))
WRAP_VOID(glClearStencil, (GLint s), (s))
WRAP_VOID(glStencilMask, (GLuint mask), (mask))
WRAP_VOID(glLoadMatrixf, (const GLfloat *m), (m))
WRAP_VOID(glDrawBuffer, (GLenum mode), (mode))

WRAP_VOID(glGenFencesNV, (GLsizei n, GLuint *fences), (n, fences))
WRAP_VOID(glDeleteFencesNV, (GLsizei n, const GLuint *fences), (n, fences))

WRAP_VOID(glCheckFramebufferStatusEXT, (GLenum target), (target));

// --- Requested Missing Functions (Batch 5) ---

// ARB Buffer / Mapping
WRAP(glMapBufferARB, GLvoid *, (GLenum target, GLenum access), (target, access))
WRAP(glUnmapBufferARB, GLboolean, (GLenum target), (target))
WRAP_VOID(glGenBuffersARB, (GLsizei n, GLuint *buffers), (n, buffers))
WRAP_VOID(glBufferSubDataARB, (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data), (target, offset, size, data))

// ARB Shader Objects (GLSL 1.x style)
WRAP(glCreateShaderObjectARB, GLhandleARB, (GLenum shaderType), (shaderType))
WRAP_VOID(glShaderSourceARB, (GLhandleARB shader, GLsizei count, const GLcharARB **string, const GLint *length),
          (shader, count, string, length))
WRAP_VOID(glCompileShaderARB, (GLhandleARB shader), (shader))
WRAP_VOID(glUseProgramObjectARB, (GLhandleARB program), (program))
WRAP_VOID(glLinkProgramARB, (GLhandleARB program), (program))
WRAP_VOID(glDeleteObjectARB, (GLhandleARB obj), (obj))
WRAP_VOID(glGetObjectParameterivARB, (GLhandleARB obj, GLenum pname, GLint *params), (obj, pname, params))
WRAP_VOID(glGetInfoLogARB, (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog), (obj, maxLength, length, infoLog))

// ARB Occlusion Queries
WRAP_VOID(glGenQueriesARB, (GLsizei n, GLuint *ids), (n, ids))
WRAP_VOID(glDeleteQueriesARB, (GLsizei n, const GLuint *ids), (n, ids))
WRAP_VOID(glBeginQueryARB, (GLenum target, GLuint id), (target, id))
WRAP_VOID(glEndQueryARB, (GLenum target), (target))
WRAP_VOID(glGetQueryObjectuivARB, (GLuint id, GLenum pname, GLuint *params), (id, pname, params))

// EXT Blend Functions
WRAP_VOID(glBlendFuncSeparateEXT, (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),
          (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))
WRAP_VOID(glBlendEquationEXT, (GLenum mode), (mode))
WRAP_VOID(glBlendColorEXT, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha))
WRAP_VOID(glBlendEquationSeparateEXT, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha))

// EXT Renderbuffer
WRAP_VOID(glGenRenderbuffersEXT, (GLsizei n, GLuint *renderbuffers), (n, renderbuffers))
WRAP_VOID(glBindRenderbufferEXT, (GLenum target, GLuint renderbuffer), (target, renderbuffer))
WRAP_VOID(glRenderbufferStorageEXT, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height),
          (target, internalformat, width, height))
WRAP_VOID(glFramebufferRenderbufferEXT, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer),
          (target, attachment, renderbuffertarget, renderbuffer))

// Misc ARB
WRAP_VOID(glCompressedTexSubImage2DARB,
          (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
           const GLvoid *data),
          (target, level, xoffset, yoffset, width, height, format, imageSize, data))
WRAP_VOID(glWindowPos2sARB, (GLshort x, GLshort y), (x, y))
WRAP_VOID(glDrawRangeElements, (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices),
          (mode, start, end, count, type, indices))
WRAP_VOID(glCopyPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type), (x, y, width, height, type))
WRAP_VOID(glGetTexImage, (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels), (target, level, format, type, pixels))
WRAP_VOID(glPrimitiveRestartIndexNV, (GLuint index), (index))

// Legacy Fixed-Function Pipeline
WRAP_VOID(glClipPlane, (GLenum plane, const GLdouble *equation), (plane, equation))
WRAP_VOID(glNormal3dv, (const GLdouble *v), (v))
WRAP_VOID(glNormal3d, (GLdouble nx, GLdouble ny, GLdouble nz), (nx, ny, nz))
WRAP_VOID(glLightModeliv, (GLenum pname, const GLint *params), (pname, params))
WRAP_VOID(glVertex4dv, (const GLdouble *v), (v))
WRAP_VOID(glVertex3dv, (const GLdouble *v), (v))
WRAP_VOID(glVertex2dv, (const GLdouble *v), (v))
WRAP_VOID(glTexGeni, (GLenum coord, GLenum pname, GLint param), (coord, pname, param))
WRAP_VOID(glTexGenfv, (GLenum coord, GLenum pname, const GLfloat *params), (coord, pname, params))
WRAP_VOID(glLineStipple, (GLint factor, GLushort pattern), (factor, pattern))
WRAP_VOID(glColor3dv, (const GLdouble *v), (v))
WRAP_VOID(glColor3d, (GLdouble red, GLdouble green, GLdouble blue), (red, green, blue))
WRAP_VOID(glColor4dv, (const GLdouble *v), (v))

WRAP_VOID(glGetUniformLocationARB, (GLint program, const GLchar *name), (program, name))
WRAP_VOID(glUniform1fvARB, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))
WRAP_VOID(glSecondaryColor3fv, (const GLfloat *v), (v))
WRAP_VOID(glUniformMatrix3fvARB, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value),
          (location, count, transpose, value))
WRAP_VOID(glUniform4fvARB, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))
WRAP_VOID(glUniform1fARB, (GLint location, GLfloat value), (location, value))
WRAP_VOID(glSecondaryColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer), (size, type, stride, pointer))
WRAP_VOID(glLockArraysEXT, (GLint first, GLsizei count), (first, count))
WRAP_VOID(glMultiDrawElements, (GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount),
          (mode, count, type, indices, drawcount))
WRAP_VOID(glUnlockArraysEXT, (), ())
WRAP_VOID(glUniform1iARB, (GLint location, GLint value), (location, value))
WRAP_VOID(glVertexAttrib3fARB, (GLuint index, GLfloat x, GLfloat y, GLfloat z), (index, x, y, z))
WRAP_VOID(glDetachObjectARB, (GLhandleARB containerObj, GLhandleARB attachedObj), (containerObj, attachedObj))
WRAP_VOID(glAttachObjectARB, (GLhandleARB containerObj, GLhandleARB obj), (containerObj, obj))
WRAP_VOID(glCreateProgramObjectARB, (), ())
WRAP_VOID(glBindAttribLocationARB, (GLhandleARB programObj, GLuint index, const GLchar *name), (programObj, index, name))
WRAP_VOID(glUniformMatrix4fvARB, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value),
          (location, count, transpose, value))
WRAP_VOID(glColor3fv, (const GLfloat *v), (v))

WRAP_VOID(glGenProgramsNV, (GLsizei n, GLuint *programs), (n, programs))
WRAP_VOID(glDeleteProgramsNV, (GLsizei n, const GLuint *programs), (n, programs))
WRAP_VOID(glBindProgramNV, (GLenum target, GLuint program), (target, program))
WRAP_VOID(glLoadProgramNV, (GLenum target, GLuint id, GLsizei len, const GLubyte *program), (target, id, len, program))
WRAP_VOID(glIsProgramNV, (GLuint program), (program))
WRAP_VOID(glTrackMatrixNV, (GLenum target, GLuint address, GLenum matrix, GLenum transform), (target, address, matrix, transform))
WRAP_VOID(glProgramParameter4fNV, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (target, index, x, y, z, w))
WRAP_VOID(glProgramParameter4fvNV, (GLenum target, GLuint index, const GLfloat *v), (target, index, v))
WRAP_VOID(glProgramParameters4fvNV, (GLenum target, GLuint index, GLsizei count, const GLfloat *v), (target, index, count, v))
WRAP_VOID(glGenOcclusionQueriesNV, (GLsizei n, GLuint *ids), (n, ids))
WRAP_VOID(glBeginOcclusionQueryNV, (GLuint id), (id))
WRAP_VOID(glEndOcclusionQueryNV, (), ())
WRAP_VOID(glGetOcclusionQueryuivNV, (GLuint id, GLenum pname, GLuint *params), (id, pname, params))

// static void my_glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *string)
// {
//     static auto real = (decltype(glProgramStringARB) *)GLHooks::GetProcAddress("glProgramStringARB");
//     if (!real) {
//         log_error("GLHooks: Missing glProgramStringARB");
//         return;
//     }

//     std::vector<char> shaderSource((const char*)string, (const char*)string + len);
//     shaderSource.push_back('\0');

//     // const char* badToken = "ARB_draw_buffers";
//     // char* found = strstr(shaderSource.data(), badToken);
//     // while (found) {
//     //     // Check if it's preceded by "OPTION "
//     //     bool isOption = false;
//     //     if (found >= shaderSource.data() + 7) {
//     //         if (strncmp(found - 7, "OPTION ", 7) == 0) {
//     //             isOption = true;
//     //         }
//     //     }

//     //     if (!isOption) {
//     //         printf("GLHooks: Patching invalid 'ARB_draw_buffers' in shader");
//     //         found[0] = '#';
//     //     }
//     //     found = strstr(found + 1, badToken);
//     // }

//     if (string) {
//         static int s_shaderCount = 0;
//         char filename[64];
//         snprintf(filename, sizeof(filename), "shader_%03d.arb", s_shaderCount++);

//         FILE* f = fopen(filename, "wb");
//         if (f) {
//             fwrite(shaderSource.data(), 1, len, f);
//             fclose(f);
//             printf("GLHooks: Dumped shader source to %s (len=%d)\n", filename, len);
//         }
//     }

//     real(target, format, len, shaderSource.data());

// }

// --- Manual Wrapper Macros ---
#ifndef WRAP_MANUAL_DEFINED
#define WRAP_MANUAL_DEFINED

// Standard Typedefs (in case specific extensions are missing)
#if !defined(GL_NV_half_float) && !defined(GL_HALF_NV)
typedef unsigned short GLhalfNV;
#endif
#if !defined(GL_EXT_timer_query) && !defined(GL_INT64_EXT)
typedef long long GLint64EXT;
typedef unsigned long long GLuint64EXT;
#endif

// Helper to cast function pointers without needing gl.h declarations
#define WRAP_MANUAL(name, ret, args, params, arg_types)                                                                                    \
    static ret my_##name args                                                                                                              \
    {                                                                                                                                      \
        typedef ret(*func_ptr) arg_types;                                                                                                  \
        static auto real = (func_ptr)my_GetProcAddress(#name);                                                                             \
        if (!real)                                                                                                                         \
        {                                                                                                                                  \
            log_error("GLHooks: Missing " #name);                                                                                          \
            return (ret)0;                                                                                                                 \
        }                                                                                                                                  \
        return real params;                                                                                                                \
    }

#define WRAP_MANUAL_VOID(name, args, params, arg_types)                                                                                    \
    static void my_##name args                                                                                                             \
    {                                                                                                                                      \
        typedef void(*func_ptr) arg_types;                                                                                                 \
        static auto real = (func_ptr)my_GetProcAddress(#name);                                                                             \
        if (!real)                                                                                                                         \
        {                                                                                                                                  \
            log_error("GLHooks: Missing " #name);                                                                                          \
            return;                                                                                                                        \
        }                                                                                                                                  \
        real params;                                                                                                                       \
    }
#endif

// WRAP_MANUAL_VOID(glAccum, (GLenum op, GLfloat value), (op, value), (GLenum, GLfloat))
// WRAP_MANUAL(glAreTexturesResident, GLboolean, (GLsizei n, const GLuint *textures, GLboolean *residences), (n, textures, residences),
// (GLsizei, const GLuint *, GLboolean *)) WRAP_MANUAL_VOID(glArrayElement, (GLint i), (i), (GLint)) WRAP_MANUAL_VOID(glBeginQuery, (GLenum
// target, GLuint id), (target, id), (GLenum, GLuint)) WRAP_MANUAL_VOID(glBindAttribLocation, (GLuint program, GLuint index, const GLchar
// *name), (program, index, name), (GLuint, GLuint, const GLchar *)) WRAP_MANUAL_VOID(glBitmap, (GLsizei width, GLsizei height, GLfloat
// xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap), (width, height, xorig, yorig, xmove, ymove, bitmap),
// (GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte *)) WRAP_MANUAL_VOID(glBlitFramebuffer, (GLint srcX0, GLint srcY0,
// GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter), (srcX0, srcY0, srcX1,
// srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter), (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum))
// WRAP_MANUAL_VOID(glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data), (target, offset, size, data),
// (GLenum, GLintptr, GLsizeiptr, const GLvoid *)) WRAP_MANUAL_VOID(glClearAccum, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha),
// (red, green, blue, alpha), (GLfloat, GLfloat, GLfloat, GLfloat)) WRAP_MANUAL_VOID(glClearIndex, (GLfloat c), (c), (GLfloat))
// WRAP_MANUAL_VOID(glColor3b, (GLbyte r, GLbyte g, GLbyte b), (r, g, b), (GLbyte, GLbyte, GLbyte))
// WRAP_MANUAL_VOID(glColor3bv, (const GLbyte *v), (v), (const GLbyte *))
// WRAP_MANUAL_VOID(glColor3i, (GLint r, GLint g, GLint b), (r, g, b), (GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glColor3iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glColor3s, (GLshort r, GLshort g, GLshort b), (r, g, b), (GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glColor3sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glColor3ubv, (const GLubyte *v), (v), (const GLubyte *))
// WRAP_MANUAL_VOID(glColor3ui, (GLuint r, GLuint g, GLuint b), (r, g, b), (GLuint, GLuint, GLuint))
// WRAP_MANUAL_VOID(glColor3uiv, (const GLuint *v), (v), (const GLuint *))
// WRAP_MANUAL_VOID(glColor3us, (GLushort r, GLushort g, GLushort b), (r, g, b), (GLushort, GLushort, GLushort))
// WRAP_MANUAL_VOID(glColor3usv, (const GLushort *v), (v), (const GLushort *))
// WRAP_MANUAL_VOID(glColor4b, (GLbyte r, GLbyte g, GLbyte b, GLbyte a), (r, g, b, a), (GLbyte, GLbyte, GLbyte, GLbyte))
// WRAP_MANUAL_VOID(glColor4bv, (const GLbyte *v), (v), (const GLbyte *))
// WRAP_MANUAL_VOID(glColor4d, (GLdouble r, GLdouble g, GLdouble b, GLdouble a), (r, g, b, a), (GLdouble, GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glColor4i, (GLint r, GLint g, GLint b, GLint a), (r, g, b, a), (GLint, GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glColor4iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glColor4s, (GLshort r, GLshort g, GLshort b, GLshort a), (r, g, b, a), (GLshort, GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glColor4sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glColor4ui, (GLuint r, GLuint g, GLuint b, GLuint a), (r, g, b, a), (GLuint, GLuint, GLuint, GLuint))
// WRAP_MANUAL_VOID(glColor4uiv, (const GLuint *v), (v), (const GLuint *))
// WRAP_MANUAL_VOID(glColor4us, (GLushort r, GLushort g, GLushort b, GLushort a), (r, g, b, a), (GLushort, GLushort, GLushort, GLushort))
// WRAP_MANUAL_VOID(glColor4usv, (const GLushort *v), (v), (const GLushort *))
// WRAP_MANUAL_VOID(glCompressedTexImage1D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei
// imageSize, const GLvoid *data), (target, level, internalformat, width, border, imageSize, data), (GLenum, GLint, GLenum, GLsizei, GLint,
// GLsizei, const GLvoid *)) WRAP_MANUAL_VOID(glCompressedTexImage3D, (GLenum target, GLint level, GLenum internalformat, GLsizei width,
// GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data), (target, level, internalformat, width, height,
// depth, border, imageSize, data), (GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *))
// WRAP_MANUAL_VOID(glCompressedTexSubImage1D, (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize,
// const GLvoid *data), (target, level, xoffset, width, format, imageSize, data), (GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const
// GLvoid *)) WRAP_MANUAL_VOID(glCompressedTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei
// height, GLenum format, GLsizei imageSize, const GLvoid *data), (target, level, xoffset, yoffset, width, height, format, imageSize, data),
// (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid *)) WRAP_MANUAL_VOID(glCompressedTexSubImage3D, (GLenum
// target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei
// imageSize, const GLvoid *data), (target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data), (GLenum,
// GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid *)) WRAP_MANUAL_VOID(glCopyTexImage1D, (GLenum
// target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border), (target, level, internalformat, x, y, width,
// border), (GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLint)) WRAP_MANUAL_VOID(glCopyTexImage2D, (GLenum target, GLint level, GLenum
// internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border), (target, level, internalformat, x, y, width, height,
// border), (GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint)) WRAP_MANUAL_VOID(glCopyTexSubImage1D, (GLenum target, GLint
// level, GLint xoffset, GLint x, GLint y, GLsizei width), (target, level, xoffset, x, y, width), (GLenum, GLint, GLint, GLint, GLint,
// GLsizei)) WRAP_MANUAL_VOID(glCopyTexSubImage3D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint
// y, GLsizei width, GLsizei height), (target, level, xoffset, yoffset, zoffset, x, y, width, height), (GLenum, GLint, GLint, GLint, GLint,
// GLint, GLint, GLsizei, GLsizei)) WRAP_MANUAL_VOID(glDeleteQueries, (GLsizei n, const GLuint *ids), (n, ids), (GLsizei, const GLuint *))
// WRAP_MANUAL_VOID(glDrawInstancedArraysNVX, (GLenum mode, GLint first, GLsizei count, GLsizei primcount), (mode, first, count, primcount),
// (GLenum, GLint, GLsizei, GLsizei)) WRAP_MANUAL_VOID(glDrawInstancedElementsNVX, (GLenum mode, GLsizei count, GLenum type, const GLvoid
// *indices, GLsizei primcount), (mode, count, type, indices, primcount), (GLenum, GLsizei, GLenum, const GLvoid *, GLsizei))
// WRAP_MANUAL_VOID(glDrawPixels, (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels), (width, height, format,
// type, pixels), (GLsizei, GLsizei, GLenum, GLenum, const GLvoid *)) WRAP_MANUAL_VOID(glEdgeFlag, (GLboolean flag), (flag), (GLboolean))
// WRAP_MANUAL_VOID(glEdgeFlagPointer, (GLsizei stride, const GLvoid *pointer), (stride, pointer), (GLsizei, const GLvoid *))
// WRAP_MANUAL_VOID(glEdgeFlagv, (const GLboolean *flag), (flag), (const GLboolean *))
// WRAP_MANUAL_VOID(glEndQuery, (GLenum target), (target), (GLenum))
// WRAP_MANUAL_VOID(glEvalCoord1d, (GLdouble u), (u), (GLdouble))
// WRAP_MANUAL_VOID(glEvalCoord1dv, (const GLdouble *u), (u), (const GLdouble *))
// WRAP_MANUAL_VOID(glEvalCoord1f, (GLfloat u), (u), (GLfloat))
// WRAP_MANUAL_VOID(glEvalCoord1fv, (const GLfloat *u), (u), (const GLfloat *))
// WRAP_MANUAL_VOID(glEvalCoord2d, (GLdouble u, GLdouble v), (u, v), (GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glEvalCoord2dv, (const GLdouble *u), (u), (const GLdouble *))
// WRAP_MANUAL_VOID(glEvalCoord2f, (GLfloat u, GLfloat v), (u, v), (GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glEvalCoord2fv, (const GLfloat *u), (u), (const GLfloat *))
// WRAP_MANUAL_VOID(glEvalMesh1, (GLenum mode, GLint i1, GLint i2), (mode, i1, i2), (GLenum, GLint, GLint))
// WRAP_MANUAL_VOID(glEvalMesh2, (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2), (mode, i1, i2, j1, j2), (GLenum, GLint, GLint,
// GLint, GLint)) WRAP_MANUAL_VOID(glEvalPoint1, (GLint i), (i), (GLint)) WRAP_MANUAL_VOID(glEvalPoint2, (GLint i, GLint j), (i, j), (GLint,
// GLint)) WRAP_MANUAL_VOID(glFeedbackBuffer, (GLsizei size, GLenum type, GLfloat *buffer), (size, type, buffer), (GLsizei, GLenum, GLfloat
// *)) WRAP_MANUAL_VOID(glFogiv, (GLenum pname, const GLint *params), (pname, params), (GLenum, const GLint *))
// WRAP_MANUAL_VOID(glFramebufferTexture1D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target,
// attachment, textarget, texture, level), (GLenum, GLenum, GLenum, GLuint, GLint)) WRAP_MANUAL_VOID(glFramebufferTexture3D, (GLenum target,
// GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset), (target, attachment, textarget, texture, level,
// zoffset), (GLenum, GLenum, GLenum, GLuint, GLint, GLint)) WRAP_MANUAL_VOID(glGenQueries, (GLsizei n, GLuint *ids), (n, ids), (GLsizei,
// GLuint *)) WRAP_MANUAL_VOID(glGenerateMipmap, (GLenum target), (target), (GLenum)) WRAP_MANUAL_VOID(glGetActiveAttrib, (GLuint program,
// GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name), (program, index, bufSize, length, size, type,
// name), (GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLchar *)) WRAP_MANUAL_VOID(glGetAttachedObjects, (GLuint program, GLsizei
// maxCount, GLsizei *count, GLuint *obj), (program, maxCount, count, obj), (GLuint, GLsizei, GLsizei *, GLuint *))
// WRAP_MANUAL_VOID(glGetBufferParameteriv, (GLenum target, GLenum pname, GLint *params), (target, pname, params), (GLenum, GLenum, GLint
// *)) WRAP_MANUAL_VOID(glGetBufferPointerv, (GLenum target, GLenum pname, GLvoid **params), (target, pname, params), (GLenum, GLenum,
// GLvoid * *)) WRAP_MANUAL_VOID(glGetBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, GLvoid *data), (target, offset, size,
// data), (GLenum, GLintptr, GLsizeiptr, GLvoid *)) WRAP_MANUAL_VOID(glGetClipPlane, (GLenum plane, GLdouble *equation), (plane, equation),
// (GLenum, GLdouble *)) WRAP_MANUAL_VOID(glGetCompressedTexImage, (GLenum target, GLint level, GLvoid *img), (target, level, img), (GLenum,
// GLint, GLvoid *)) WRAP_MANUAL_VOID(glGetFenceivNV, (GLuint fence, GLenum pname, GLint *params), (fence, pname, params), (GLuint, GLenum,
// GLint *)) WRAP_MANUAL_VOID(glGetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint *params),
// (target, attachment, pname, params), (GLenum, GLenum, GLenum, GLint *)) WRAP_MANUAL(glGetHandle, GLuint, (GLenum pname), (pname),
// (GLenum)) WRAP_MANUAL_VOID(glGetLightiv, (GLenum pname, GLint *params), (pname, params), (GLenum, GLint *)) WRAP_MANUAL_VOID(glGetMapdv,
// (GLenum target, GLenum query, GLdouble *v), (target, query, v), (GLenum, GLenum, GLdouble *)) WRAP_MANUAL_VOID(glGetMapfv, (GLenum
// target, GLenum query, GLfloat *v), (target, query, v), (GLenum, GLenum, GLfloat *)) WRAP_MANUAL_VOID(glGetMapiv, (GLenum target, GLenum
// query, GLint *v), (target, query, v), (GLenum, GLenum, GLint *)) WRAP_MANUAL_VOID(glGetMaterialiv, (GLenum pname, GLint *params), (pname,
// params), (GLenum, GLint *)) WRAP_MANUAL_VOID(glGetObjectParameterfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat
// *)) WRAP_MANUAL_VOID(glGetPixelMapfv, (GLenum map, GLfloat *values), (map, values), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetPixelMapuiv, (GLenum map, GLuint *values), (map, values), (GLenum, GLuint *))
// WRAP_MANUAL_VOID(glGetPixelMapusv, (GLenum map, GLushort *values), (map, values), (GLenum, GLushort *))
// WRAP_MANUAL_VOID(glGetPointerv, (GLenum pname, GLvoid **params), (pname, params), (GLenum, GLvoid * *))
// WRAP_MANUAL_VOID(glGetPolygonStipple, (GLubyte *mask), (mask), (GLubyte *))
// WRAP_MANUAL_VOID(glGetProgramEnvParameterdv, (GLenum pname, GLdouble *params), (pname, params), (GLenum, GLdouble *))
// WRAP_MANUAL_VOID(glGetProgramEnvParameterfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetProgramLocalParameterdv, (GLenum pname, GLdouble *params), (pname, params), (GLenum, GLdouble *))
// WRAP_MANUAL_VOID(glGetProgramLocalParameterfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetProgramString, (GLenum target, GLenum pname, GLvoid *string), (target, pname, string), (GLenum, GLenum, GLvoid *))
// WRAP_MANUAL_VOID(glGetQueryObjecti64v, (GLuint id, GLenum pname, GLint64EXT *params), (id, pname, params), (GLuint, GLenum, GLint64EXT
// *)) WRAP_MANUAL_VOID(glGetQueryObjectiv, (GLuint id, GLenum pname, GLint *params), (id, pname, params), (GLuint, GLenum, GLint *))
// WRAP_MANUAL_VOID(glGetQueryObjectui64v, (GLuint id, GLenum pname, GLuint64EXT *params), (id, pname, params), (GLuint, GLenum, GLuint64EXT
// *)) WRAP_MANUAL_VOID(glGetQueryObjectuiv, (GLuint id, GLenum pname, GLuint *params), (id, pname, params), (GLuint, GLenum, GLuint *))
// WRAP_MANUAL_VOID(glGetQueryiv, (GLenum target, GLenum pname, GLint *params), (target, pname, params), (GLenum, GLenum, GLint *))
// WRAP_MANUAL_VOID(glGetRenderbufferParameteriv, (GLenum target, GLenum pname, GLint *params), (target, pname, params), (GLenum, GLenum,
// GLint *)) WRAP_MANUAL_VOID(glGetShaderSource, (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source), (shader, bufSize,
// length, source), (GLuint, GLsizei, GLsizei *, GLchar *)) WRAP_MANUAL_VOID(glGetTexEnvfv, (GLenum pname, GLfloat *params), (pname,
// params), (GLenum, GLfloat *)) WRAP_MANUAL_VOID(glGetTexEnviv, (GLenum pname, GLint *params), (pname, params), (GLenum, GLint *))
// WRAP_MANUAL_VOID(glGetTexGendv, (GLenum pname, GLdouble *params), (pname, params), (GLenum, GLdouble *))
// WRAP_MANUAL_VOID(glGetTexGenfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetTexGeniv, (GLenum pname, GLint *params), (pname, params), (GLenum, GLint *))
// WRAP_MANUAL_VOID(glGetTexLevelParameterfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetTexParameterfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetTexParameteriv, (GLenum pname, GLint *params), (pname, params), (GLenum, GLint *))
// WRAP_MANUAL_VOID(glGetUniformfv, (GLenum pname, GLfloat *params), (pname, params), (GLenum, GLfloat *))
// WRAP_MANUAL_VOID(glGetUniformiv, (GLenum pname, GLint *params), (pname, params), (GLenum, GLint *))
// WRAP_MANUAL_VOID(glGetVertexAttribPointerv, (GLuint index, GLenum pname, GLvoid **pointer), (index, pname, pointer), (GLuint, GLenum,
// GLvoid * *)) WRAP_MANUAL_VOID(glGetVertexAttribdv, (GLuint index, GLenum pname, GLdouble *params), (index, pname, params), (GLuint,
// GLenum, GLdouble *)) WRAP_MANUAL_VOID(glGetVertexAttribfv, (GLuint index, GLenum pname, GLfloat *params), (index, pname, params),
// (GLuint, GLenum, GLfloat *)) WRAP_MANUAL_VOID(glGetVertexAttribiv, (GLuint index, GLenum pname, GLint *params), (index, pname, params),
// (GLuint, GLenum, GLint *)) WRAP_MANUAL_VOID(glIndexMask, (GLuint mask), (mask), (GLuint)) WRAP_MANUAL_VOID(glIndexPointer, (GLenum type,
// GLsizei stride, const GLvoid *pointer), (type, stride, pointer), (GLenum, GLsizei, const GLvoid *)) WRAP_MANUAL_VOID(glIndexd, (GLdouble
// c), (c), (GLdouble)) WRAP_MANUAL_VOID(glIndexdv, (const GLdouble *c), (c), (const GLdouble *)) WRAP_MANUAL_VOID(glIndexf, (GLfloat c),
// (c), (GLfloat)) WRAP_MANUAL_VOID(glIndexfv, (const GLfloat *c), (c), (const GLfloat *)) WRAP_MANUAL_VOID(glIndexi, (GLint c), (c),
// (GLint)) WRAP_MANUAL_VOID(glIndexiv, (const GLint *c), (c), (const GLint *)) WRAP_MANUAL_VOID(glIndexs, (GLshort c), (c), (GLshort))
// WRAP_MANUAL_VOID(glIndexsv, (const GLshort *c), (c), (const GLshort *))
// WRAP_MANUAL_VOID(glIndexub, (GLubyte c), (c), (GLubyte))
// WRAP_MANUAL_VOID(glIndexubv, (const GLubyte *c), (c), (const GLubyte *))
// WRAP_MANUAL_VOID(glInitNames, (), (), ())
// WRAP_MANUAL_VOID(glInstanceDivisorNVX, (GLuint index, GLuint divisor), (index, divisor), (GLuint, GLuint))
// WRAP_MANUAL(glIsBuffer, GLboolean, (GLuint buffer), (buffer), (GLuint))
// WRAP_MANUAL(glIsFramebuffer, GLboolean, (GLuint framebuffer), (framebuffer), (GLuint))
// WRAP_MANUAL(glIsProgram, GLboolean, (GLuint program), (program), (GLuint))
// WRAP_MANUAL(glIsQuery, GLboolean, (GLuint id), (id), (GLuint))
// WRAP_MANUAL(glIsRenderbuffer, GLboolean, (GLuint renderbuffer), (renderbuffer), (GLuint))
// WRAP_MANUAL_VOID(glLightModelf, (GLenum pname, GLfloat param), (pname, param), (GLenum, GLfloat))
// WRAP_MANUAL_VOID(glLighti, (GLenum light, GLenum pname, GLint param), (light, pname, param), (GLenum, GLenum, GLint))
// WRAP_MANUAL_VOID(glLightiv, (GLenum light, GLenum pname, const GLint *params), (light, pname, params), (GLenum, GLenum, const GLint *))
// WRAP_MANUAL_VOID(glListBase, (GLuint base), (base), (GLuint))
// WRAP_MANUAL_VOID(glLoadMatrixd, (const GLdouble *m), (m), (const GLdouble *))
// WRAP_MANUAL_VOID(glLoadName, (GLuint name), (name), (GLuint))
// WRAP_MANUAL_VOID(glLoadTransposeMatrixd, (const GLdouble *m), (m), (const GLdouble *))
// WRAP_MANUAL_VOID(glMap1d, (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points), (target, u1, u2,
// stride, order, points), (GLenum, GLdouble, GLdouble, GLint, GLint, const GLdouble *)) WRAP_MANUAL_VOID(glMap1f, (GLenum target, GLfloat
// u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points), (target, u1, u2, stride, order, points), (GLenum, GLfloat, GLfloat,
// GLint, GLint, const GLfloat *)) WRAP_MANUAL_VOID(glMap2d, (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble
// v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points), (target, u1, u2, ustride, uorder, v1, v2, vstride, vorder,
// points), (GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, const GLdouble *)) WRAP_MANUAL_VOID(glMap2f,
// (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat
// *points), (target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points), (GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat,
// GLint, GLint, const GLfloat *)) WRAP_MANUAL_VOID(glMapGrid1d, (GLint un, GLdouble u1, GLdouble u2), (un, u1, u2), (GLint, GLdouble,
// GLdouble)) WRAP_MANUAL_VOID(glMapGrid1f, (GLint un, GLfloat u1, GLfloat u2), (un, u1, u2), (GLint, GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glMapGrid2d, (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2), (un, u1, u2, vn, v1, v2), (GLint,
// GLdouble, GLdouble, GLint, GLdouble, GLdouble)) WRAP_MANUAL_VOID(glMapGrid2f, (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1,
// GLfloat v2), (un, u1, u2, vn, v1, v2), (GLint, GLfloat, GLfloat, GLint, GLfloat, GLfloat)) WRAP_MANUAL_VOID(glMateriali, (GLenum face,
// GLenum pname, GLint param), (face, pname, param), (GLenum, GLenum, GLint)) WRAP_MANUAL_VOID(glMaterialiv, (GLenum face, GLenum pname,
// const GLint *params), (face, pname, params), (GLenum, GLenum, const GLint *)) WRAP_MANUAL_VOID(glMultMatrixd, (const GLdouble *m), (m),
// (const GLdouble *)) WRAP_MANUAL_VOID(glMultTransposeMatrixd, (const GLdouble *m), (m), (const GLdouble *))
// WRAP_MANUAL_VOID(glMultiDrawInstancedArraysNVX, (GLenum mode, const GLint *first, const GLsizei *count, const GLsizei *primcount, GLsizei
// drawcount), (mode, first, count, primcount, drawcount), (GLenum, const GLint *, const GLsizei *, const GLsizei *, GLsizei))
// WRAP_MANUAL_VOID(glMultiDrawInstancedElementsNVX, (GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, const
// GLsizei *primcount, GLsizei drawcount), (mode, count, type, indices, primcount, drawcount), (GLenum, const GLsizei *, GLenum, const
// GLvoid *const *, const GLsizei *, GLsizei)) WRAP_MANUAL_VOID(glMultiTexCoord3f, (GLfloat s, GLfloat t, GLfloat r), (s, t, r), (GLfloat,
// GLfloat, GLfloat)) WRAP_MANUAL_VOID(glNormal3b, (GLbyte x, GLbyte y, GLbyte z), (x, y, z), (GLbyte, GLbyte, GLbyte))
// WRAP_MANUAL_VOID(glNormal3bv, (const GLbyte *v), (v), (const GLbyte *))
// WRAP_MANUAL_VOID(glNormal3i, (GLint x, GLint y, GLint z), (x, y, z), (GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glNormal3iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glNormal3s, (GLshort x, GLshort y, GLshort z), (x, y, z), (GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glNormal3sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glPassThrough, (GLfloat token), (token), (GLfloat))
// WRAP_MANUAL_VOID(glPixelMapfv, (GLenum map, GLsizei mapsize, const GLfloat *values), (map, mapsize, values), (GLenum, GLsizei, const
// GLfloat *)) WRAP_MANUAL_VOID(glPixelMapuiv, (GLenum map, GLsizei mapsize, const GLuint *values), (map, mapsize, values), (GLenum,
// GLsizei, const GLuint *)) WRAP_MANUAL_VOID(glPixelZoom, (GLfloat xfactor, GLfloat yfactor), (xfactor, yfactor), (GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glPolygonStipple, (const GLubyte *mask), (mask), (const GLubyte *))
// WRAP_MANUAL_VOID(glPopName, (), (), ())
// WRAP_MANUAL_VOID(glPrimitiveRestartNV, (), (), ())
// WRAP_MANUAL_VOID(glPrioritizeTextures, (GLsizei n, const GLuint *textures, const GLclampf *priorities), (n, textures, priorities),
// (GLsizei, const GLuint *, const GLclampf *)) WRAP_MANUAL_VOID(glProgramEnvParameter4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w),
// (x, y, z, w), (GLdouble, GLdouble, GLdouble, GLdouble)) WRAP_MANUAL_VOID(glProgramEnvParameter4dv, (const GLdouble *v), (v), (const
// GLdouble *)) WRAP_MANUAL_VOID(glProgramEnvParameters4fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glProgramLocalParameter4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), (x, y, z, w), (GLdouble, GLdouble,
// GLdouble, GLdouble)) WRAP_MANUAL_VOID(glProgramLocalParameter4dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glProgramLocalParameters4fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glPushName, (GLuint name), (name), (GLuint))
// WRAP_MANUAL_VOID(glRasterPos2d, (GLdouble x, GLdouble y), (x, y), (GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glRasterPos2dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glRasterPos2fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glRasterPos2iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glRasterPos2s, (GLshort x, GLshort y), (x, y), (GLshort, GLshort))
// WRAP_MANUAL_VOID(glRasterPos2sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glRasterPos3d, (GLdouble x, GLdouble y, GLdouble z), (x, y, z), (GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glRasterPos3dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glRasterPos3fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glRasterPos3i, (GLint x, GLint y, GLint z), (x, y, z), (GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glRasterPos3iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glRasterPos3s, (GLshort x, GLshort y, GLshort z), (x, y, z), (GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glRasterPos3sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glRasterPos4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), (x, y, z, w), (GLdouble, GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glRasterPos4dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glRasterPos4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), (x, y, z, w), (GLfloat, GLfloat, GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glRasterPos4fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glRasterPos4i, (GLint x, GLint y, GLint z, GLint w), (x, y, z, w), (GLint, GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glRasterPos4iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glRasterPos4s, (GLshort x, GLshort y, GLshort z, GLshort w), (x, y, z, w), (GLshort, GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glRasterPos4sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glRectd, (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2), (x1, y1, x2, y2), (GLdouble, GLdouble, GLdouble,
// GLdouble)) WRAP_MANUAL_VOID(glRectdv, (const GLdouble *v1, const GLdouble *v2), (v1, v2), (const GLdouble *, const GLdouble *))
// WRAP_MANUAL_VOID(glRectfv, (const GLfloat *v1, const GLfloat *v2), (v1, v2), (const GLfloat *, const GLfloat *))
// WRAP_MANUAL_VOID(glRectiv, (const GLint *v1, const GLint *v2), (v1, v2), (const GLint *, const GLint *))
// WRAP_MANUAL_VOID(glRects, (GLshort x1, GLshort y1, GLshort x2, GLshort y2), (x1, y1, x2, y2), (GLshort, GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glRectsv, (const GLshort *v1, const GLshort *v2), (v1, v2), (const GLshort *, const GLshort *))
// WRAP_MANUAL(glRenderMode, GLint, (GLenum mode), (mode), (GLenum))
// WRAP_MANUAL_VOID(glRenderbufferStorageMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei
// height), (target, samples, internalformat, width, height), (GLenum, GLsizei, GLenum, GLsizei, GLsizei))
// WRAP_MANUAL_VOID(glRenderbufferStorageMultisampleCoverageNV, (GLenum target, GLsizei coverageSamples, GLsizei colorSamples, GLenum
// internalformat, GLsizei width, GLsizei height), (target, coverageSamples, colorSamples, internalformat, width, height), (GLenum, GLsizei,
// GLsizei, GLenum, GLsizei, GLsizei)) WRAP_MANUAL_VOID(glRotated, (GLdouble angle, GLdouble x, GLdouble y, GLdouble z), (angle, x, y, z),
// (GLdouble, GLdouble, GLdouble, GLdouble)) WRAP_MANUAL_VOID(glSampleCoverage, (GLclampf value, GLboolean invert), (value, invert),
// (GLclampf, GLboolean)) WRAP_MANUAL_VOID(glScaled, (GLdouble x, GLdouble y, GLdouble z), (x, y, z), (GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glSelectBuffer, (GLsizei size, GLuint *buffer), (size, buffer), (GLsizei, GLuint *))
// WRAP_MANUAL(glTestFenceNV, GLboolean, (GLuint fence), (fence), (GLuint))
// WRAP_MANUAL_VOID(glTexCoord1d, (GLdouble s), (s), (GLdouble))
// WRAP_MANUAL_VOID(glTexCoord1dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glTexCoord1f, (GLfloat s), (s), (GLfloat))
// WRAP_MANUAL_VOID(glTexCoord1fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glTexCoord1i, (GLint s), (s), (GLint))
// WRAP_MANUAL_VOID(glTexCoord1iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glTexCoord1s, (GLshort s), (s), (GLshort))
// WRAP_MANUAL_VOID(glTexCoord1sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glTexCoord2d, (GLdouble s, GLdouble t), (s, t), (GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glTexCoord2dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glTexCoord2i, (GLint s, GLint t), (s, t), (GLint, GLint))
// WRAP_MANUAL_VOID(glTexCoord2iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glTexCoord2s, (GLshort s, GLshort t), (s, t), (GLshort, GLshort))
// WRAP_MANUAL_VOID(glTexCoord2sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glTexCoord3d, (GLdouble s, GLdouble t, GLdouble r), (s, t, r), (GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glTexCoord3dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glTexCoord3f, (GLfloat s, GLfloat t, GLfloat r), (s, t, r), (GLfloat, GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glTexCoord3fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glTexCoord3i, (GLint s, GLint t, GLint r), (s, t, r), (GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glTexCoord3iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glTexCoord3s, (GLshort s, GLshort t, GLshort r), (s, t, r), (GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glTexCoord3sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glTexCoord4d, (GLdouble s, GLdouble t, GLdouble r, GLdouble q), (s, t, r, q), (GLdouble, GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glTexCoord4dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glTexCoord4fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glTexCoord4i, (GLint s, GLint t, GLint r, GLint q), (s, t, r, q), (GLint, GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glTexCoord4iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glTexCoord4s, (GLshort s, GLshort t, GLshort r, GLshort q), (s, t, r, q), (GLshort, GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glTexCoord4sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glTexGend, (GLenum coord, GLenum pname, GLdouble param), (coord, pname, param), (GLenum, GLenum, GLdouble))
// WRAP_MANUAL_VOID(glTexGendv, (GLenum coord, GLenum pname, const GLdouble *params), (coord, pname, params), (GLenum, GLenum, const
// GLdouble *)) WRAP_MANUAL_VOID(glTexGenf, (GLenum coord, GLenum pname, GLfloat param), (coord, pname, param), (GLenum, GLenum, GLfloat))
// WRAP_MANUAL_VOID(glTexGeniv, (GLenum coord, GLenum pname, const GLint *params), (coord, pname, params), (GLenum, GLenum, const GLint *))
// WRAP_MANUAL_VOID(glTexImage1D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum
// type, const GLvoid *pixels), (target, level, internalformat, width, border, format, type, pixels), (GLenum, GLint, GLint, GLsizei, GLint,
// GLenum, GLenum, const GLvoid *)) WRAP_MANUAL_VOID(glTexParameteriv, (GLenum target, GLenum pname, const GLint *params), (target, pname,
// params), (GLenum, GLenum, const GLint *)) WRAP_MANUAL_VOID(glTexSubImage1D, (GLenum target, GLint level, GLint xoffset, GLsizei width,
// GLenum format, GLenum type, const GLvoid *pixels), (target, level, xoffset, width, format, type, pixels), (GLenum, GLint, GLint, GLsizei,
// GLenum, GLenum, const GLvoid *)) WRAP_MANUAL_VOID(glUniform2f, (GLint location, GLfloat v0, GLfloat v1), (location, v0, v1), (GLint,
// GLfloat, GLfloat)) WRAP_MANUAL_VOID(glUniform2i, (GLint location, GLint v0, GLint v1), (location, v0, v1), (GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glUniform3f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2), (location, v0, v1, v2), (GLint, GLfloat, GLfloat,
// GLfloat)) WRAP_MANUAL_VOID(glUniform3i, (GLint location, GLint v0, GLint v1, GLint v2), (location, v0, v1, v2), (GLint, GLint, GLint,
// GLint)) WRAP_MANUAL_VOID(glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3), (location, v0, v1, v2, v3),
// (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) WRAP_MANUAL_VOID(glUniform4i, (GLint location, GLint v0, GLint v1, GLint v2, GLint v3),
// (location, v0, v1, v2, v3), (GLint, GLint, GLint, GLint, GLint)) WRAP_MANUAL_VOID(glValidateProgram, (GLuint program), (program),
// (GLuint)) WRAP_MANUAL_VOID(glVertex2fv, (const GLfloat *v), (v), (const GLfloat *)) WRAP_MANUAL_VOID(glVertex2iv, (const GLint *v), (v),
// (const GLint *)) WRAP_MANUAL_VOID(glVertex2s, (GLshort x, GLshort y), (x, y), (GLshort, GLshort)) WRAP_MANUAL_VOID(glVertex2sv, (const
// GLshort *v), (v), (const GLshort *)) WRAP_MANUAL_VOID(glVertex3d, (GLdouble x, GLdouble y, GLdouble z), (x, y, z), (GLdouble, GLdouble,
// GLdouble)) WRAP_MANUAL_VOID(glVertex3i, (GLint x, GLint y, GLint z), (x, y, z), (GLint, GLint, GLint)) WRAP_MANUAL_VOID(glVertex3iv,
// (const GLint *v), (v), (const GLint *)) WRAP_MANUAL_VOID(glVertex3s, (GLshort x, GLshort y, GLshort z), (x, y, z), (GLshort, GLshort,
// GLshort)) WRAP_MANUAL_VOID(glVertex3sv, (const GLshort *v), (v), (const GLshort *)) WRAP_MANUAL_VOID(glVertex4d, (GLdouble x, GLdouble y,
// GLdouble z, GLdouble w), (x, y, z, w), (GLdouble, GLdouble, GLdouble, GLdouble)) WRAP_MANUAL_VOID(glVertex4fv, (const GLfloat *v), (v),
// (const GLfloat *)) WRAP_MANUAL_VOID(glVertex4i, (GLint x, GLint y, GLint z, GLint w), (x, y, z, w), (GLint, GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glVertex4iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glVertex4s, (GLshort x, GLshort y, GLshort z, GLshort w), (x, y, z, w), (GLshort, GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glVertex4sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glVertexAttrib1d, (GLuint index, GLdouble x), (index, x), (GLuint, GLdouble))
// WRAP_MANUAL_VOID(glVertexAttrib1dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glVertexAttrib1fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glVertexAttrib1hNV, (GLuint index, GLhalfNV x), (index, x), (GLuint, GLhalfNV))
// WRAP_MANUAL_VOID(glVertexAttrib1hvNV, (GLuint index, const GLhalfNV *v), (index, v), (GLuint, const GLhalfNV *))
// WRAP_MANUAL_VOID(glVertexAttrib1s, (GLuint index, GLshort x), (index, x), (GLuint, GLshort))
// WRAP_MANUAL_VOID(glVertexAttrib1sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glVertexAttrib2d, (GLuint index, GLdouble x, GLdouble y), (index, x, y), (GLuint, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glVertexAttrib2dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glVertexAttrib2f, (GLuint index, GLfloat x, GLfloat y), (index, x, y), (GLuint, GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glVertexAttrib2fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glVertexAttrib2hNV, (GLuint index, GLhalfNV x, GLhalfNV y), (index, x, y), (GLuint, GLhalfNV, GLhalfNV))
// WRAP_MANUAL_VOID(glVertexAttrib2hvNV, (GLuint index, const GLhalfNV *v), (index, v), (GLuint, const GLhalfNV *))
// WRAP_MANUAL_VOID(glVertexAttrib2s, (GLuint index, GLshort x, GLshort y), (index, x, y), (GLuint, GLshort, GLshort))
// WRAP_MANUAL_VOID(glVertexAttrib2sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glVertexAttrib3d, (GLuint index, GLdouble x, GLdouble y, GLdouble z), (index, x, y, z), (GLuint, GLdouble, GLdouble,
// GLdouble)) WRAP_MANUAL_VOID(glVertexAttrib3dv, (const GLdouble *v), (v), (const GLdouble *)) WRAP_MANUAL_VOID(glVertexAttrib3f, (GLuint
// index, GLfloat x, GLfloat y, GLfloat z), (index, x, y, z), (GLuint, GLfloat, GLfloat, GLfloat)) WRAP_MANUAL_VOID(glVertexAttrib3fv,
// (const GLfloat *v), (v), (const GLfloat *)) WRAP_MANUAL_VOID(glVertexAttrib3hNV, (GLuint index, GLhalfNV x, GLhalfNV y, GLhalfNV z),
// (index, x, y, z), (GLuint, GLhalfNV, GLhalfNV, GLhalfNV)) WRAP_MANUAL_VOID(glVertexAttrib3hvNV, (GLuint index, const GLhalfNV *v),
// (index, v), (GLuint, const GLhalfNV *)) WRAP_MANUAL_VOID(glVertexAttrib3s, (GLuint index, GLshort x, GLshort y, GLshort z), (index, x, y,
// z), (GLuint, GLshort, GLshort, GLshort)) WRAP_MANUAL_VOID(glVertexAttrib3sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glVertexAttrib4Nbv, (GLuint index, const GLbyte *v), (index, v), (GLuint, const GLbyte *))
// WRAP_MANUAL_VOID(glVertexAttrib4Niv, (GLuint index, const GLint *v), (index, v), (GLuint, const GLint *))
// WRAP_MANUAL_VOID(glVertexAttrib4Nsv, (GLuint index, const GLshort *v), (index, v), (GLuint, const GLshort *))
// WRAP_MANUAL_VOID(glVertexAttrib4Nubv, (GLuint index, const GLubyte *v), (index, v), (GLuint, const GLubyte *))
// WRAP_MANUAL_VOID(glVertexAttrib4Nuiv, (GLuint index, const GLuint *v), (index, v), (GLuint, const GLuint *))
// WRAP_MANUAL_VOID(glVertexAttrib4Nusv, (GLuint index, const GLushort *v), (index, v), (GLuint, const GLushort *))
// WRAP_MANUAL_VOID(glVertexAttrib4bv, (const GLbyte *v), (v), (const GLbyte *))
// WRAP_MANUAL_VOID(glVertexAttrib4d, (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w), (index, x, y, z, w), (GLuint,
// GLdouble, GLdouble, GLdouble, GLdouble)) WRAP_MANUAL_VOID(glVertexAttrib4dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glVertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (index, x, y, z, w), (GLuint, GLfloat,
// GLfloat, GLfloat, GLfloat)) WRAP_MANUAL_VOID(glVertexAttrib4fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glVertexAttrib4hNV, (GLuint index, GLhalfNV x, GLhalfNV y, GLhalfNV z, GLhalfNV w), (index, x, y, z, w), (GLuint,
// GLhalfNV, GLhalfNV, GLhalfNV, GLhalfNV)) WRAP_MANUAL_VOID(glVertexAttrib4hvNV, (GLuint index, const GLhalfNV *v), (index, v), (GLuint,
// const GLhalfNV *)) WRAP_MANUAL_VOID(glVertexAttrib4iv, (const GLint *v), (v), (const GLint *)) WRAP_MANUAL_VOID(glVertexAttrib4s, (GLuint
// index, GLshort x, GLshort y, GLshort z, GLshort w), (index, x, y, z, w), (GLuint, GLshort, GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glVertexAttrib4sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glVertexAttrib4ubv, (const GLubyte *v), (v), (const GLubyte *))
// WRAP_MANUAL_VOID(glVertexAttrib4uiv, (const GLuint *v), (v), (const GLuint *))
// WRAP_MANUAL_VOID(glVertexAttrib4usv, (const GLushort *v), (v), (const GLushort *))
// WRAP_MANUAL_VOID(glVertexAttribs1hvNV, (GLuint index, GLsizei n, const GLhalfNV *v), (index, n, v), (GLuint, GLsizei, const GLhalfNV *))
// WRAP_MANUAL_VOID(glVertexAttribs2hvNV, (GLuint index, GLsizei n, const GLhalfNV *v), (index, n, v), (GLuint, GLsizei, const GLhalfNV *))
// WRAP_MANUAL_VOID(glVertexAttribs3hvNV, (GLuint index, GLsizei n, const GLhalfNV *v), (index, n, v), (GLuint, GLsizei, const GLhalfNV *))
// WRAP_MANUAL_VOID(glVertexAttribs4hvNV, (GLuint index, GLsizei n, const GLhalfNV *v), (index, n, v), (GLuint, GLsizei, const GLhalfNV *))
// WRAP_MANUAL_VOID(glWindowPos2d, (GLdouble x, GLdouble y), (x, y), (GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glWindowPos2dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glWindowPos2f, (GLfloat x, GLfloat y), (x, y), (GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glWindowPos2fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glWindowPos2i, (GLint x, GLint y), (x, y), (GLint, GLint))
// WRAP_MANUAL_VOID(glWindowPos2iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glWindowPos2s, (GLshort x, GLshort y), (x, y), (GLshort, GLshort))
// WRAP_MANUAL_VOID(glWindowPos2sv, (const GLshort *v), (v), (const GLshort *))
// WRAP_MANUAL_VOID(glWindowPos3d, (GLdouble x, GLdouble y, GLdouble z), (x, y, z), (GLdouble, GLdouble, GLdouble))
// WRAP_MANUAL_VOID(glWindowPos3dv, (const GLdouble *v), (v), (const GLdouble *))
// WRAP_MANUAL_VOID(glWindowPos3f, (GLfloat x, GLfloat y, GLfloat z), (x, y, z), (GLfloat, GLfloat, GLfloat))
// WRAP_MANUAL_VOID(glWindowPos3fv, (const GLfloat *v), (v), (const GLfloat *))
// WRAP_MANUAL_VOID(glWindowPos3i, (GLint x, GLint y, GLint z), (x, y, z), (GLint, GLint, GLint))
// WRAP_MANUAL_VOID(glWindowPos3iv, (const GLint *v), (v), (const GLint *))
// WRAP_MANUAL_VOID(glWindowPos3s, (GLshort x, GLshort y, GLshort z), (x, y, z), (GLshort, GLshort, GLshort))
// WRAP_MANUAL_VOID(glWindowPos3sv, (const GLshort *v), (v), (const GLshort *))

// ID5
WRAP_MANUAL_VOID(glProgramEnvParameters4fvEXT, (GLenum target, GLuint index, GLsizei count, const GLfloat *params),
                 (target, index, count, params), (GLenum, GLuint, GLsizei, const GLfloat *))
WRAP_MANUAL_VOID(glProgramLocalParameters4fvEXT, (GLenum target, GLuint index, GLsizei count, const GLfloat *params),
                 (target, index, count, params), (GLenum, GLuint, GLsizei, const GLfloat *))

// My implementations

static void my_glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    static auto real = (decltype(glViewport) *)SDL_GL_GetProcAddress("glViewport");
    if (!real)
    {
        log_error("GLHooks: Missing glViewport");
        return;
    }
    void *retAddr = GET_RETURN_ADDRESS();

    if (retAddr == (void *)0x0813043e || retAddr == (void *)0x08130adf)
    {
        return;
    }

    // w = 640;
    // h = 480;

    real(x, y, w, h);
    log_debug("glViewport(%d, %d, %d, %d)", x, y, w, h);
}

// -----------------------------------------------------------------------------
// GetProcAddress:
// This function maps string names (e.g., "glBegin") to our static wrapper functions.
// This manual mapping is required because C++ does not support reflection to lookup
// static functions by name at runtime.
// IMPORTANT: If you add a WRAP() above, you MUST add a corresponding MAP() here!
// -----------------------------------------------------------------------------
void *GLHooks::GetProcAddress(const char *procName)
{
#define MAP(name)                                                                                                                          \
    if (strcmp(procName, #name) == 0)                                                                                                      \
    return (void *)&my_##name

    MAP(glBegin);
    MAP(glEnd);
    MAP(glVertex3f);
    MAP(glColor4f);
    MAP(glEnable);
    MAP(glDisable);
    MAP(glIsEnabled);
    MAP(glMatrixMode);
    MAP(glLoadIdentity);
    MAP(glPushMatrix);
    MAP(glPopMatrix);
    MAP(glOrtho);
    MAP(glTranslated);
    MAP(glTranslatef);
    MAP(glRotatef);
    MAP(glScalef);
    MAP(glViewport);
    MAP(glClear);
    MAP(glClearColor);
    MAP(glBindTexture);
    MAP(glDeleteTextures);
    MAP(glGenTextures);
    MAP(glTexParameteri);
    MAP(glTexImage2D);
    MAP(glGetLightfv);

    // New mappings
    MAP(glCullFace);
    MAP(glFrontFace);
    MAP(glHint);
    MAP(glLineWidth);
    MAP(glPointSize);
    MAP(glPolygonMode);
    MAP(glScissor);
    MAP(glShadeModel);
    MAP(glPixelStorei);
    MAP(glPixelStoref);
    MAP(glGetBooleanv);
    MAP(glGetDoublev);
    MAP(glGetFloatv);
    MAP(glGetIntegerv);
    MAP(glPushAttrib);
    MAP(glPopAttrib);
    MAP(glPushClientAttrib);
    MAP(glPopClientAttrib);
    MAP(glAlphaFunc);
    MAP(glBlendFunc);
    MAP(glLogicOp);
    MAP(glStencilFunc);
    MAP(glStencilOp);
    MAP(glDepthFunc);
    MAP(glDepthMask);
    MAP(glLightfv);
    MAP(glLightf);
    MAP(glLightModelfv);
    MAP(glMaterialfv);
    MAP(glNormal3f);
    MAP(glTexEnvf);
    MAP(glTexEnvi);
    MAP(glTexEnvfv);
    MAP(glTexEnviv);

    // Geometry & Lists
    MAP(glTexCoord2f);
    MAP(glTexCoord2fv);
    MAP(glActiveTexture);
    MAP(glMultiTexCoord2f);
    MAP(glMultiTexCoord2fv);
    MAP(glActiveTextureARB);
    MAP(glMultiTexCoord2fARB);
    MAP(glMultiTexCoord2fvARB);
    MAP(glVertex2d);
    MAP(glVertex2f);
    MAP(glVertex2i);
    MAP(glVertex3fv);
    MAP(glRasterPos2i);
    MAP(glRasterPos3f);
    MAP(glDepthRange);
    MAP(glPolygonOffset);
    MAP(glFlush);
    MAP(glFinish);
    MAP(glColor3f);
    MAP(glColor4ub);
    MAP(glColor4fv);
    MAP(glRectf);
    MAP(glRecti);
    MAP(glNewList);
    MAP(glEndList);
    MAP(glCallList);
    MAP(glGenLists);
    MAP(glDeleteLists);
    MAP(glIsList);
    MAP(glGetString);
    MAP(glGetError);
    MAP(glFrustum);
    MAP(glEnableClientState);
    MAP(glDisableClientState);
    MAP(glClientActiveTexture);
    MAP(glVertexPointer);
    MAP(glTexCoordPointer);
    MAP(glNormalPointer);
    MAP(glColorPointer);
    MAP(glDrawArrays);
    MAP(glDrawElements);

    // Batch 1 Registration
    MAP(glBindBuffer);
    MAP(glDeleteBuffers);
    MAP(glGenBuffers);
    MAP(glBufferData);
    MAP(glGenFramebuffers);
    MAP(glBindFramebuffer);
    MAP(glFramebufferTexture2D);
    MAP(glCheckFramebufferStatus);
    MAP(glDeleteFramebuffers);
    MAP(glFramebufferRenderbuffer);
    MAP(glGenRenderbuffers);
    MAP(glBindRenderbuffer);
    MAP(glRenderbufferStorage);
    MAP(glDeleteRenderbuffers);
    MAP(glDrawBuffers);
    MAP(glReadPixels);
    MAP(glCreateShader);
    MAP(glShaderSource);
    MAP(glCompileShader);
    MAP(glGetShaderiv);
    MAP(glGetShaderInfoLog);
    MAP(glDeleteShader);
    MAP(glCreateProgram);
    MAP(glAttachShader);
    MAP(glLinkProgram);
    MAP(glGetProgramiv);
    MAP(glGetProgramInfoLog);
    MAP(glUseProgram);
    MAP(glDeleteProgram);
    MAP(glGetUniformLocation);
    MAP(glGetAttribLocation);
    MAP(glUniform1f);
    MAP(glUniform1fv);
    MAP(glUniform1i);
    MAP(glUniform1iv);
    MAP(glUniform2fv);
    MAP(glUniform2iv);
    MAP(glUniform3fv);
    MAP(glUniform3iv);
    MAP(glUniform4fv);
    MAP(glUniform4iv);
    MAP(glUniformMatrix2fv);
    MAP(glUniformMatrix3fv);
    MAP(glUniformMatrix4fv);
    MAP(glGetActiveUniform);
    MAP(glEnableVertexAttribArray);
    MAP(glDisableVertexAttribArray);
    MAP(glVertexAttribPointer);
    MAP(glTexParameterf);
    MAP(glTexSubImage2D);
    MAP(glColorMask);
    MAP(glClearDepth);
    MAP(glClearStencil);
    MAP(glStencilMask);
    MAP(glLoadMatrixf);
    MAP(glDrawBuffer);
    MAP(glPixelMapusv);
    MAP(glPixelTransferf);
    MAP(glPixelTransferi);
    MAP(glInterleavedArrays);

    // Batch 2 Registration
    MAP(glColor3ub);
    MAP(glFogfv);
    MAP(glFogf);
    MAP(glColor3fv);

    // Restored MAPs for non-NV functions
    MAP(glTexCoord4f);
    MAP(glVertex4f);
    MAP(glCompressedTexImage2DARB);
    MAP(glGenFramebuffersEXT);
    MAP(glBlendColor);
    MAP(glMultMatrixf);
    MAP(glGenProgramsARB);
    MAP(glBindFramebufferEXT);
    MAP(glProgramStringARB);
    MAP(glClientActiveTextureARB);
    MAP(glVertexAttribPointerARB);
    MAP(glDeleteFramebuffersEXT);
    MAP(glPointParameterf);
    MAP(glProgramLocalParameter4fARB);
    MAP(glProgramLocalParameter4fvARB);
    MAP(glColorMaterial);
    MAP(glFogi);
    MAP(glMaterialf);
    MAP(glLightModeli);
    MAP(glTexParameterfv);
    MAP(glEnableVertexAttribArrayARB);
    MAP(glPointParameterfv);
    MAP(glBindProgramARB);
    MAP(glGetProgramivARB);
    MAP(glFramebufferTexture2DEXT);
    MAP(glSecondaryColor3ub);
    MAP(glBlendEquation);
    MAP(glDisableVertexAttribArrayARB);
    MAP(glIsProgramARB);
    MAP(glGenFencesNV);
    MAP(glDeleteFencesNV);
    MAP(glCheckFramebufferStatusEXT);
    MAP(glGetUniformLocationARB);
    MAP(glUniform1fvARB);
    MAP(glSecondaryColor3fv);
    MAP(glUniformMatrix3fvARB);
    MAP(glUniform4fvARB);
    MAP(glUniform1fARB);
    MAP(glSecondaryColorPointer);
    MAP(glLockArraysEXT);
    MAP(glMultiDrawElements);
    MAP(glUnlockArraysEXT);
    MAP(glUniform1iARB);
    MAP(glVertexAttrib3fARB);
    MAP(glDetachObjectARB);
    MAP(glAttachObjectARB);
    MAP(glCreateProgramObjectARB);
    MAP(glBindAttribLocationARB);
    MAP(glUniformMatrix4fvARB);

    MAP(glGenProgramsNV);
    MAP(glDeleteProgramsNV);
    MAP(glBindProgramNV);
    MAP(glLoadProgramNV);
    MAP(glIsProgramNV);
    MAP(glTrackMatrixNV);
    MAP(glProgramParameter4fNV);
    MAP(glProgramParameter4fvNV);
    MAP(glProgramParameters4fvNV);
    MAP(glGenOcclusionQueriesNV);
    MAP(glBeginOcclusionQueryNV);
    MAP(glEndOcclusionQueryNV);
    MAP(glGetOcclusionQueryuivNV);

    // // NV Extensions
    // if (strcmp(procName, "glGenProgramsNV") == 0)
    //     return (void *)&ShaderPatches::glGenProgramsNV;
    // if (strcmp(procName, "glDeleteProgramsNV") == 0)
    //     return (void *)&ShaderPatches::glDeleteProgramsNV;
    // if (strcmp(procName, "glBindProgramNV") == 0)
    //     return (void *)&ShaderPatches::glBindProgramNV;
    // if (strcmp(procName, "glLoadProgramNV") == 0)
    //     return (void *)&ShaderPatches::glLoadProgramNV;
    // if (strcmp(procName, "glIsProgramNV") == 0)
    //     return (void *)&ShaderPatches::glIsProgramNV;
    // // if (strcmp(procName, "glTrackMatrixNV") == 0)
    // //     return (void *)&ShaderPatches::glTrackMatrixNV;
    // if (strcmp(procName, "glProgramParameter4fNV") == 0)
    //     return (void *)&ShaderPatches::glProgramParameter4fNV;
    // if (strcmp(procName, "glProgramParameter4fvNV") == 0)
    //     return (void *)&ShaderPatches::glProgramParameter4fvNV;
    // if (strcmp(procName, "glProgramParameters4fvNV") == 0)
    //     return (void *)&ShaderPatches::glProgramParameters4fvNV;

    // // Extensions: Occlusion Query (NV)
    // if (strcmp(procName, "glGenOcclusionQueriesNV") == 0)
    //     return (void *)&ShaderPatches::glGenOcclusionQueriesNV;
    // if (strcmp(procName, "glBeginOcclusionQueryNV") == 0)
    //     return (void *)&ShaderPatches::glBeginOcclusionQueryNV;
    // if (strcmp(procName, "glEndOcclusionQueryNV") == 0)
    //     return (void *)&ShaderPatches::glEndOcclusionQueryNV;
    // if (strcmp(procName, "glGetOcclusionQueryuivNV") == 0)
    //     return (void *)&ShaderPatches::glGetOcclusionQueryuivNV;

    // Batch 4 Registration
    MAP(glSetFenceNV);
    MAP(glIsTexture);
    MAP(glMultTransposeMatrixf);
    MAP(glColor4ubv);
    MAP(glCompressedTexImage2D);
    MAP(glMapBuffer);
    MAP(glFinishFenceNV);
    MAP(glVertexAttrib1f);
    MAP(glClampColorARB);
    MAP(glBlendFuncSeparate);
    MAP(glGetBufferParameterivARB);
    MAP(glDeleteProgramsARB);
    MAP(glGetTexLevelParameteriv);
    MAP(glRasterPos2f);
    MAP(glDeleteRenderbuffersEXT);
    MAP(glDeleteBuffersARB);
    MAP(glCopyTexSubImage2D);
    MAP(glBufferDataARB);
    MAP(glBindBufferARB);
    MAP(glIsBufferARB);
    MAP(glLoadTransposeMatrixf);
    MAP(glGetMaterialfv);
    MAP(glUnmapBuffer);
    MAP(glProgramEnvParameter4fARB);
    MAP(glReadBuffer);
    MAP(glIsFenceNV);
    MAP(glProgramEnvParameter4fvARB);

    // Batch 5 Registration
    MAP(glMapBufferARB);
    MAP(glUnmapBufferARB);
    MAP(glGenBuffersARB);
    MAP(glBufferSubDataARB);
    MAP(glCreateShaderObjectARB);
    MAP(glShaderSourceARB);
    MAP(glCompileShaderARB);
    MAP(glUseProgramObjectARB);
    MAP(glLinkProgramARB);
    MAP(glDeleteObjectARB);
    MAP(glGetObjectParameterivARB);
    MAP(glGetInfoLogARB);
    MAP(glGenQueriesARB);
    MAP(glDeleteQueriesARB);
    MAP(glBeginQueryARB);
    MAP(glEndQueryARB);
    MAP(glGetQueryObjectuivARB);
    MAP(glBlendFuncSeparateEXT);
    MAP(glBlendEquationEXT);
    MAP(glBlendColorEXT);
    MAP(glBlendEquationSeparateEXT);
    MAP(glGenRenderbuffersEXT);
    MAP(glBindRenderbufferEXT);
    MAP(glRenderbufferStorageEXT);
    MAP(glFramebufferRenderbufferEXT);
    MAP(glCompressedTexSubImage2DARB);
    MAP(glWindowPos2sARB);
    MAP(glDrawRangeElements);
    MAP(glCopyPixels);
    MAP(glGetTexImage);
    MAP(glPrimitiveRestartIndexNV);
    MAP(glClipPlane);
    MAP(glNormal3dv);
    MAP(glNormal3d);
    MAP(glLightModeliv);
    MAP(glVertex4dv);
    MAP(glVertex3dv);
    MAP(glVertex2dv);
    MAP(glTexGeni);
    MAP(glTexGenfv);
    MAP(glLineStipple);
    MAP(glColor3dv);
    MAP(glColor3d);
    MAP(glColor4dv);

    // MAP(glAccum);
    // MAP(glAreTexturesResident);
    // MAP(glArrayElement);
    // MAP(glBeginQuery);
    // MAP(glBindAttribLocation);
    // MAP(glBitmap);
    // MAP(glBlitFramebuffer);
    // MAP(glBufferSubData);
    // MAP(glClearAccum);
    // MAP(glClearIndex);
    // MAP(glColor3b);
    // MAP(glColor3bv);
    // MAP(glColor3i);
    // MAP(glColor3iv);
    // MAP(glColor3s);
    // MAP(glColor3sv);
    // MAP(glColor3ubv);
    // MAP(glColor3ui);
    // MAP(glColor3uiv);
    // MAP(glColor3us);
    // MAP(glColor3usv);
    // MAP(glColor4b);
    // MAP(glColor4bv);
    // MAP(glColor4d);
    // MAP(glColor4i);
    // MAP(glColor4iv);
    // MAP(glColor4s);
    // MAP(glColor4sv);
    // MAP(glColor4ui);
    // MAP(glColor4uiv);
    // MAP(glColor4us);
    // MAP(glColor4usv);
    // MAP(glCompressedTexImage1D);
    // MAP(glCompressedTexImage3D);
    // MAP(glCompressedTexSubImage1D);
    // MAP(glCompressedTexSubImage2D);
    // MAP(glCompressedTexSubImage3D);
    // MAP(glCopyTexImage1D);
    // MAP(glCopyTexImage2D);
    // MAP(glCopyTexSubImage1D);
    // MAP(glCopyTexSubImage3D);
    // MAP(glDeleteQueries);
    // MAP(glDrawInstancedArraysNVX);
    // MAP(glDrawInstancedElementsNVX);
    // MAP(glDrawPixels);
    // MAP(glEdgeFlag);
    // MAP(glEdgeFlagPointer);
    // MAP(glEdgeFlagv);
    // MAP(glEndQuery);
    // MAP(glEvalCoord1d);
    // MAP(glEvalCoord1dv);
    // MAP(glEvalCoord1f);
    // MAP(glEvalCoord1fv);
    // MAP(glEvalCoord2d);
    // MAP(glEvalCoord2dv);
    // MAP(glEvalCoord2f);
    // MAP(glEvalCoord2fv);
    // MAP(glEvalMesh1);
    // MAP(glEvalMesh2);
    // MAP(glEvalPoint1);
    // MAP(glEvalPoint2);
    // MAP(glFeedbackBuffer);
    // MAP(glFogiv);
    // MAP(glFramebufferTexture1D);
    // MAP(glFramebufferTexture3D);
    // MAP(glGenQueries);
    // MAP(glGenerateMipmap);
    // MAP(glGetActiveAttrib);
    // MAP(glGetAttachedObjects);
    // MAP(glGetBufferParameteriv);
    // MAP(glGetBufferPointerv);
    // MAP(glGetBufferSubData);
    // MAP(glGetClipPlane);
    // MAP(glGetCompressedTexImage);
    // MAP(glGetFenceivNV);
    // MAP(glGetFramebufferAttachmentParameteriv);
    // MAP(glGetHandle);
    // MAP(glGetLightiv);
    // MAP(glGetMapdv);
    // MAP(glGetMapfv);
    // MAP(glGetMapiv);
    // MAP(glGetMaterialiv);
    // MAP(glGetObjectParameterfv);
    // MAP(glGetPixelMapfv);
    // MAP(glGetPixelMapuiv);
    // MAP(glGetPixelMapusv);
    // MAP(glGetPointerv);
    // MAP(glGetPolygonStipple);
    // MAP(glGetProgramEnvParameterdv);
    // MAP(glGetProgramEnvParameterfv);
    // MAP(glGetProgramLocalParameterdv);
    // MAP(glGetProgramLocalParameterfv);
    // MAP(glGetProgramString);
    // MAP(glGetQueryObjecti64v);
    // MAP(glGetQueryObjectiv);
    // MAP(glGetQueryObjectui64v);
    // MAP(glGetQueryObjectuiv);
    // MAP(glGetQueryiv);
    // MAP(glGetRenderbufferParameteriv);
    // MAP(glGetShaderSource);
    // MAP(glGetTexEnvfv);
    // MAP(glGetTexEnviv);
    // MAP(glGetTexGendv);
    // MAP(glGetTexGenfv);
    // MAP(glGetTexGeniv);
    // MAP(glGetTexLevelParameterfv);
    // MAP(glGetTexParameterfv);
    // MAP(glGetTexParameteriv);
    // MAP(glGetUniformfv);
    // MAP(glGetUniformiv);
    // MAP(glGetVertexAttribPointerv);
    // MAP(glGetVertexAttribdv);
    // MAP(glGetVertexAttribfv);
    // MAP(glGetVertexAttribiv);
    // MAP(glIndexMask);
    // MAP(glIndexPointer);
    // MAP(glIndexd);
    // MAP(glIndexdv);
    // MAP(glIndexf);
    // MAP(glIndexfv);
    // MAP(glIndexi);
    // MAP(glIndexiv);
    // MAP(glIndexs);
    // MAP(glIndexsv);
    // MAP(glIndexub);
    // MAP(glIndexubv);
    // MAP(glInitNames);
    // MAP(glInstanceDivisorNVX);
    // MAP(glIsBuffer);
    // MAP(glIsFramebuffer);
    // MAP(glIsProgram);
    // MAP(glIsQuery);
    // MAP(glIsRenderbuffer);
    // MAP(glLightModelf);
    // MAP(glLighti);
    // MAP(glLightiv);
    // MAP(glListBase);
    // MAP(glLoadMatrixd);
    // MAP(glLoadName);
    // MAP(glLoadTransposeMatrixd);
    // MAP(glMap1d);
    // MAP(glMap1f);
    // MAP(glMap2d);
    // MAP(glMap2f);
    // MAP(glMapGrid1d);
    // MAP(glMapGrid1f);
    // MAP(glMapGrid2d);
    // MAP(glMapGrid2f);
    // MAP(glMateriali);
    // MAP(glMaterialiv);
    // MAP(glMultMatrixd);
    // MAP(glMultTransposeMatrixd);
    // MAP(glMultiDrawInstancedArraysNVX);
    // MAP(glMultiDrawInstancedElementsNVX);
    // MAP(glMultiTexCoord3f);
    // MAP(glNormal3b);
    // MAP(glNormal3bv);
    // MAP(glNormal3i);
    // MAP(glNormal3iv);
    // MAP(glNormal3s);
    // MAP(glNormal3sv);
    // MAP(glPassThrough);
    // MAP(glPixelMapfv);
    // MAP(glPixelMapuiv);
    // MAP(glPixelZoom);
    // MAP(glPolygonStipple);
    // MAP(glPopName);
    // MAP(glPrimitiveRestartNV);
    // MAP(glPrioritizeTextures);
    // MAP(glProgramEnvParameter4d);
    // MAP(glProgramEnvParameter4dv);
    // MAP(glProgramEnvParameters4fv);
    // MAP(glProgramLocalParameter4d);
    // MAP(glProgramLocalParameter4dv);
    // MAP(glProgramLocalParameters4fv);
    // MAP(glPushName);
    // MAP(glRasterPos2d);
    // MAP(glRasterPos2dv);
    // MAP(glRasterPos2fv);
    // MAP(glRasterPos2iv);
    // MAP(glRasterPos2s);
    // MAP(glRasterPos2sv);
    // MAP(glRasterPos3d);
    // MAP(glRasterPos3dv);
    // MAP(glRasterPos3fv);
    // MAP(glRasterPos3i);
    // MAP(glRasterPos3iv);
    // MAP(glRasterPos3s);
    // MAP(glRasterPos3sv);
    // MAP(glRasterPos4d);
    // MAP(glRasterPos4dv);
    // MAP(glRasterPos4f);
    // MAP(glRasterPos4fv);
    // MAP(glRasterPos4i);
    // MAP(glRasterPos4iv);
    // MAP(glRasterPos4s);
    // MAP(glRasterPos4sv);
    // MAP(glRectd);
    // MAP(glRectdv);
    // MAP(glRectfv);
    // MAP(glRectiv);
    // MAP(glRects);
    // MAP(glRectsv);
    // MAP(glRenderMode);
    // MAP(glRenderbufferStorageMultisample);
    // MAP(glRenderbufferStorageMultisampleCoverageNV);
    // MAP(glRotated);
    // MAP(glSampleCoverage);
    // MAP(glScaled);
    // MAP(glSelectBuffer);
    // MAP(glTestFenceNV);
    // MAP(glTexCoord1d);
    // MAP(glTexCoord1dv);
    // MAP(glTexCoord1f);
    // MAP(glTexCoord1fv);
    // MAP(glTexCoord1i);
    // MAP(glTexCoord1iv);
    // MAP(glTexCoord1s);
    // MAP(glTexCoord1sv);
    // MAP(glTexCoord2d);
    // MAP(glTexCoord2dv);
    // MAP(glTexCoord2i);
    // MAP(glTexCoord2iv);
    // MAP(glTexCoord2s);
    // MAP(glTexCoord2sv);
    // MAP(glTexCoord3d);
    // MAP(glTexCoord3dv);
    // MAP(glTexCoord3f);
    // MAP(glTexCoord3fv);
    // MAP(glTexCoord3i);
    // MAP(glTexCoord3iv);
    // MAP(glTexCoord3s);
    // MAP(glTexCoord3sv);
    // MAP(glTexCoord4d);
    // MAP(glTexCoord4dv);
    // MAP(glTexCoord4fv);
    // MAP(glTexCoord4i);
    // MAP(glTexCoord4iv);
    // MAP(glTexCoord4s);
    // MAP(glTexCoord4sv);
    // MAP(glTexGend);
    // MAP(glTexGendv);
    // MAP(glTexGenf);
    // MAP(glTexGeniv);
    // MAP(glTexImage1D);
    // MAP(glTexParameteriv);
    // MAP(glTexSubImage1D);
    // MAP(glUniform2f);
    // MAP(glUniform2i);
    // MAP(glUniform3f);
    // MAP(glUniform3i);
    // MAP(glUniform4f);
    // MAP(glUniform4i);
    // MAP(glValidateProgram);
    // MAP(glVertex2fv);
    // MAP(glVertex2iv);
    // MAP(glVertex2s);
    // MAP(glVertex2sv);
    // MAP(glVertex3d);
    // MAP(glVertex3i);
    // MAP(glVertex3iv);
    // MAP(glVertex3s);
    // MAP(glVertex3sv);
    // MAP(glVertex4d);
    // MAP(glVertex4fv);
    // MAP(glVertex4i);
    // MAP(glVertex4iv);
    // MAP(glVertex4s);
    // MAP(glVertex4sv);
    // MAP(glVertexAttrib1d);
    // MAP(glVertexAttrib1dv);
    // MAP(glVertexAttrib1fv);
    // MAP(glVertexAttrib1hNV);
    // MAP(glVertexAttrib1hvNV);
    // MAP(glVertexAttrib1s);
    // MAP(glVertexAttrib1sv);
    // MAP(glVertexAttrib2d);
    // MAP(glVertexAttrib2dv);
    // MAP(glVertexAttrib2f);
    // MAP(glVertexAttrib2fv);
    // MAP(glVertexAttrib2hNV);
    // MAP(glVertexAttrib2hvNV);
    // MAP(glVertexAttrib2s);
    // MAP(glVertexAttrib2sv);
    // MAP(glVertexAttrib3d);
    // MAP(glVertexAttrib3dv);
    // MAP(glVertexAttrib3f);
    // MAP(glVertexAttrib3fv);
    // MAP(glVertexAttrib3hNV);
    // MAP(glVertexAttrib3hvNV);
    // MAP(glVertexAttrib3s);
    // MAP(glVertexAttrib3sv);
    // MAP(glVertexAttrib4Nbv);
    // MAP(glVertexAttrib4Niv);
    // MAP(glVertexAttrib4Nsv);
    // MAP(glVertexAttrib4Nubv);
    // MAP(glVertexAttrib4Nuiv);
    // MAP(glVertexAttrib4Nusv);
    // MAP(glVertexAttrib4bv);
    // MAP(glVertexAttrib4d);
    // MAP(glVertexAttrib4dv);
    // MAP(glVertexAttrib4f);
    // MAP(glVertexAttrib4fv);
    // MAP(glVertexAttrib4hNV);
    // MAP(glVertexAttrib4hvNV);
    // MAP(glVertexAttrib4iv);
    // MAP(glVertexAttrib4s);
    // MAP(glVertexAttrib4sv);
    // MAP(glVertexAttrib4ubv);
    // MAP(glVertexAttrib4uiv);
    // MAP(glVertexAttrib4usv);
    // MAP(glVertexAttribs1hvNV);
    // MAP(glVertexAttribs2hvNV);
    // MAP(glVertexAttribs3hvNV);
    // MAP(glVertexAttribs4hvNV);
    // MAP(glWindowPos2d);
    // MAP(glWindowPos2dv);
    // MAP(glWindowPos2f);
    // MAP(glWindowPos2fv);
    // MAP(glWindowPos2i);
    // MAP(glWindowPos2iv);
    // MAP(glWindowPos2s);
    // MAP(glWindowPos2sv);
    // MAP(glWindowPos3d);
    // MAP(glWindowPos3dv);
    // MAP(glWindowPos3f);
    // MAP(glWindowPos3fv);
    // MAP(glWindowPos3i);
    // MAP(glWindowPos3iv);
    // MAP(glWindowPos3s);
    // MAP(glWindowPos3sv);

    // ID5
    MAP(glProgramEnvParameters4fvEXT);
    MAP(glProgramLocalParameters4fvEXT);

    return NULL;
}