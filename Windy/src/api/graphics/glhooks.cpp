#define WIN32_LEAN_AND_MEAN
// Ensure we get prototypes for GL extensions (glActiveTexture, etc)
#define GL_GLEXT_PROTOTYPES
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h> // Provides declarations for GL > 1.1
#include <GL/glu.h>

#include "glhooks.h"
#include "glxbridge.h"
#include "shaderpatches.h"
#include "../../core/log.h"
#include <string.h>

// Macros for efficient wrapper generation
#define WRAP(name, ret, args, params)                                                                                                      \
    static ret my_##name args                                                                                                              \
    {                                                                                                                                      \
        static auto real = (decltype(name) *)GLXBridge::GetProcAddress(#name);                                                             \
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
        static auto real = (decltype(name) *)GLXBridge::GetProcAddress(#name);                                                             \
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
WRAP_VOID(glViewport, (GLint x, GLint y, GLsizei w, GLsizei h), (x, y, w, h))
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
WRAP_VOID(glCompressedTexImage2DARB, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data), (target, level, internalformat, width, height, border, imageSize, data))
WRAP_VOID(glTexParameterfv, (GLenum target, GLenum pname, const GLfloat *params), (target, pname, params))
WRAP_VOID(glPointParameterf, (GLenum pname, GLfloat param), (pname, param))
WRAP_VOID(glPointParameterfv, (GLenum pname, const GLfloat *params), (pname, params))
static void my_glPointParameterfARB(GLenum pname, GLfloat param)
{
    using ArbFn = void (*)(GLenum, GLfloat);
    static ArbFn real = nullptr;
    if (!real)
    {
        real = (ArbFn)GLXBridge::GetProcAddress("glPointParameterfARB");
        if (!real)
            real = (ArbFn)GLXBridge::GetProcAddress("glPointParameterf");
        if (!real)
            real = (ArbFn)GLXBridge::GetProcAddress("glPointParameterfEXT");
    }
    if (!real)
    {
        log_error("GLHooks: Missing glPointParameterfARB");
        return;
    }
    real(pname, param);
}

// Vertex / Matrix
WRAP_VOID(glVertex4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), (x, y, z, w))
WRAP_VOID(glMultMatrixf, (const GLfloat *m), (m))

// GLU
WRAP(gluErrorString, const GLubyte *, (GLenum error), (error))
WRAP_VOID(gluOrtho2D, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top), (left, right, bottom, top))

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
WRAP_VOID(glProgramLocalParameter4fARB, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (target, index, x, y, z, w))
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
         (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels),
         (target, level, xoffset, yoffset, width, height, format, type, pixels)) 

// Misc
WRAP_VOID(glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha), (red, green, blue, alpha))
WRAP_VOID(glClearDepth, (GLclampd depth), (depth))
WRAP_VOID(glClearStencil, (GLint s), (s))
WRAP_VOID(glStencilMask, (GLuint mask), (mask))
WRAP_VOID(glLoadMatrixf, (const GLfloat *m), (m))
WRAP_VOID(glDrawBuffer, (GLenum mode), (mode))

// ARB_vertex_buffer_object (ARB variants)
WRAP(glMapBufferARB, void *, (GLenum target, GLenum access), (target, access))
WRAP(glUnmapBufferARB, GLboolean, (GLenum target), (target))
WRAP_VOID(glBindBufferARB, (GLenum target, GLuint buffer), (target, buffer))
WRAP_VOID(glDeleteBuffersARB, (GLsizei n, const GLuint *buffers), (n, buffers))
WRAP_VOID(glGenBuffersARB, (GLsizei n, GLuint *buffers), (n, buffers))
WRAP_VOID(glBufferDataARB, (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage), (target, size, data, usage))
WRAP_VOID(glBufferSubDataARB, (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data), (target, offset, size, data))

// ARB_occlusion_query
WRAP_VOID(glGenQueriesARB, (GLsizei n, GLuint *ids), (n, ids))
WRAP_VOID(glDeleteQueriesARB, (GLsizei n, const GLuint *ids), (n, ids))
WRAP_VOID(glBeginQueryARB, (GLenum target, GLuint id), (target, id))
WRAP_VOID(glEndQueryARB, (GLenum target), (target))
WRAP_VOID(glGetQueryObjectuivARB, (GLuint id, GLenum pname, GLuint *params), (id, pname, params))
static void my_glGetQueryObjectivARB(GLuint id, GLenum pname, GLint *params)
{
    using ArbFn = void (*)(GLuint, GLenum, GLint *);
    static ArbFn real = nullptr;
    if (!real)
    {
        real = (ArbFn)GLXBridge::GetProcAddress("glGetQueryObjectivARB");
        if (!real)
            real = (ArbFn)GLXBridge::GetProcAddress("glGetQueryObjectiv");
        if (!real)
            real = (ArbFn)GLXBridge::GetProcAddress("glGetQueryObjectivEXT");
    }
    if (!real)
    {
        log_error("GLHooks: Missing glGetQueryObjectivARB");
        return;
    }
    real(id, pname, params);
}

static void my_glDrawArraysEXT(GLenum mode, GLint first, GLsizei count)
{
    using ExtFn = void (*)(GLenum, GLint, GLsizei);
    static ExtFn real = nullptr;
    if (!real)
    {
        real = (ExtFn)GLXBridge::GetProcAddress("glDrawArraysEXT");
        if (!real)
            real = (ExtFn)GLXBridge::GetProcAddress("glDrawArrays");
    }
    if (!real)
    {
        log_error("GLHooks: Missing glDrawArraysEXT");
        return;
    }
    real(mode, first, count);
}

// GL 1.2 / 1.0 core (missing)
WRAP_VOID(glDrawRangeElements, (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices),
          (mode, start, end, count, type, indices))
WRAP_VOID(glCopyPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type), (x, y, width, height, type))
WRAP_VOID(glCopyTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height),
          (target, level, xoffset, yoffset, x, y, width, height))
WRAP_VOID(glGetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint *params), (target, level, pname, params))
WRAP_VOID(glGetTexImage, (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels), (target, level, format, type, pixels))

// NV_primitive_restart
WRAP_VOID(glPrimitiveRestartIndexNV, (GLuint index), (index))

// ARB_vertex_program (missing entries)
WRAP_VOID(glDeleteProgramsARB, (GLsizei n, const GLuint *programs), (n, programs))
WRAP_VOID(glProgramEnvParameter4fvARB, (GLenum target, GLuint index, const GLfloat *params), (target, index, params))

// EXT_blend extensions
WRAP_VOID(glBlendFuncSeparateEXT, (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),
          (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))
WRAP_VOID(glBlendEquationEXT, (GLenum mode), (mode))
WRAP_VOID(glBlendColorEXT, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha))
WRAP_VOID(glBlendEquationSeparateEXT, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha))

// ARB_window_pos
WRAP_VOID(glWindowPos2sARB, (GLshort x, GLshort y), (x, y))

// ARB_shader_objects (old-style GLhandleARB API, mapped to GLuint)
WRAP(glCreateShaderObjectARB, GLuint, (GLenum shaderType), (shaderType))
WRAP_VOID(glShaderSourceARB, (GLuint shaderObj, GLsizei count, const GLchar **string, const GLint *length),
          (shaderObj, count, string, length))
WRAP_VOID(glCompileShaderARB, (GLuint shaderObj), (shaderObj))
WRAP_VOID(glUseProgramObjectARB, (GLuint programObj), (programObj))
WRAP_VOID(glDeleteObjectARB, (GLuint obj), (obj))
WRAP_VOID(glLinkProgramARB, (GLuint programObj), (programObj))
WRAP_VOID(glGetObjectParameterivARB, (GLuint obj, GLenum pname, GLint *params), (obj, pname, params))
WRAP_VOID(glGetInfoLogARB, (GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog), (obj, maxLength, length, infoLog))

// EXT_framebuffer_object (missing renderbuffer entries)
WRAP_VOID(glDeleteRenderbuffersEXT, (GLsizei n, const GLuint *renderbuffers), (n, renderbuffers))
WRAP_VOID(glGenRenderbuffersEXT, (GLsizei n, GLuint *renderbuffers), (n, renderbuffers))
WRAP(glCheckFramebufferStatusEXT, GLenum, (GLenum target), (target))
WRAP_VOID(glBindRenderbufferEXT, (GLenum target, GLuint renderbuffer), (target, renderbuffer))
WRAP_VOID(glRenderbufferStorageEXT, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height),
          (target, internalformat, width, height))
WRAP_VOID(glFramebufferRenderbufferEXT, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer),
          (target, attachment, renderbuffertarget, renderbuffer))

// ARB_texture_compression
WRAP_VOID(glCompressedTexSubImage2DARB,
          (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
           const GLvoid *data),
          (target, level, xoffset, yoffset, width, height, format, imageSize, data))

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

    // Batch 2 Registration
    MAP(glColor3ub);
    MAP(glFogfv);
    MAP(glFogf);
    MAP(gluErrorString);

    // Restored MAPs for non-NV functions
    MAP(glTexCoord4f);
    MAP(glVertex4f);
    MAP(glCompressedTexImage2DARB);
    MAP(glGenFramebuffersEXT);
    MAP(glBlendColor);
    MAP(glMultMatrixf);
    MAP(glGenProgramsARB);
    MAP(glBindFramebufferEXT);
    MAP(gluOrtho2D);
    MAP(glProgramStringARB);
    MAP(glClientActiveTextureARB);
    MAP(glVertexAttribPointerARB);
    MAP(glDeleteFramebuffersEXT);
    MAP(glPointParameterf);
    MAP(glPointParameterfARB);
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

    // ARB_vertex_buffer_object (ARB variants)
    MAP(glMapBufferARB);
    MAP(glUnmapBufferARB);
    MAP(glBindBufferARB);
    MAP(glDeleteBuffersARB);
    MAP(glGenBuffersARB);
    MAP(glBufferDataARB);
    MAP(glBufferSubDataARB);

    // ARB_occlusion_query
    MAP(glGenQueriesARB);
    MAP(glDeleteQueriesARB);
    MAP(glBeginQueryARB);
    MAP(glEndQueryARB);
    MAP(glGetQueryObjectuivARB);
    MAP(glGetQueryObjectivARB);
    MAP(glDrawArraysEXT);

    // GL core
    MAP(glDrawRangeElements);
    MAP(glCopyPixels);
    MAP(glCopyTexSubImage2D);
    MAP(glGetTexLevelParameteriv);
    MAP(glGetTexImage);

    // NV_primitive_restart
    MAP(glPrimitiveRestartIndexNV);

    // ARB_vertex_program (missing)
    MAP(glDeleteProgramsARB);
    MAP(glProgramEnvParameter4fvARB);

    // EXT_blend extensions
    MAP(glBlendFuncSeparateEXT);
    MAP(glBlendEquationEXT);
    MAP(glBlendColorEXT);
    MAP(glBlendEquationSeparateEXT);

    // ARB_window_pos
    MAP(glWindowPos2sARB);

    // ARB_shader_objects (old-style)
    MAP(glCreateShaderObjectARB);
    MAP(glShaderSourceARB);
    MAP(glCompileShaderARB);
    MAP(glUseProgramObjectARB);
    MAP(glDeleteObjectARB);
    MAP(glLinkProgramARB);
    MAP(glGetObjectParameterivARB);
    MAP(glGetInfoLogARB);

    // EXT_framebuffer_object
    MAP(glDeleteRenderbuffersEXT);
    MAP(glGenRenderbuffersEXT);
    MAP(glCheckFramebufferStatusEXT);
    MAP(glBindRenderbufferEXT);
    MAP(glRenderbufferStorageEXT);
    MAP(glFramebufferRenderbufferEXT);

    // ARB_texture_compression
    MAP(glCompressedTexSubImage2DARB);

    // Extensions: Vertex Programs (NV) - Mapped to ShaderPatches
    if (strcmp(procName, "glGenProgramsNV") == 0)
        return (void *)&ShaderPatches::glGenProgramsNV;
    if (strcmp(procName, "glDeleteProgramsNV") == 0)
        return (void *)&ShaderPatches::glDeleteProgramsNV;
    if (strcmp(procName, "glBindProgramNV") == 0)
        return (void *)&ShaderPatches::glBindProgramNV;
    if (strcmp(procName, "glLoadProgramNV") == 0)
        return (void *)&ShaderPatches::glLoadProgramNV;
    if (strcmp(procName, "glIsProgramNV") == 0)
        return (void *)&ShaderPatches::glIsProgramNV;
    if (strcmp(procName, "glTrackMatrixNV") == 0)
        return (void *)&ShaderPatches::glTrackMatrixNV;
    if (strcmp(procName, "glProgramParameter4fNV") == 0)
        return (void *)&ShaderPatches::glProgramParameter4fNV;
    if (strcmp(procName, "glProgramParameter4fvNV") == 0)
        return (void *)&ShaderPatches::glProgramParameter4fvNV;
    if (strcmp(procName, "glProgramParameters4fvNV") == 0)
        return (void *)&ShaderPatches::glProgramParameters4fvNV;

    // Extensions: Occlusion Query (NV)
    if (strcmp(procName, "glGenOcclusionQueriesNV") == 0)
        return (void *)&ShaderPatches::glGenOcclusionQueriesNV;
    if (strcmp(procName, "glBeginOcclusionQueryNV") == 0)
        return (void *)&ShaderPatches::glBeginOcclusionQueryNV;
    if (strcmp(procName, "glEndOcclusionQueryNV") == 0)
        return (void *)&ShaderPatches::glEndOcclusionQueryNV;
    if (strcmp(procName, "glGetOcclusionQueryuivNV") == 0)
        return (void *)&ShaderPatches::glGetOcclusionQueryuivNV;

    return NULL;
}
