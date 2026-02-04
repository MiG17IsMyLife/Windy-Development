// Re-implementation of GLHooks using standard C++ wrappers.
// These wrappers automatically handle the ABI transition from the Linux Host (CDECL)
// to the Windows OpenGL Driver (STDCALL).

#include "glhooks.h"
#include "glxbridge.h"
#include "../../core/log.h"
#include <string.h>

#define WIN32_LEAN_AND_MEAN
// Ensure we get prototypes for GL extensions (glActiveTexture, etc)
#define GL_GLEXT_PROTOTYPES
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h> // Provides declarations for GL > 1.1

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
WRAP_VOID(glVertex2f, (GLfloat x, GLfloat y), (x, y))
WRAP_VOID(glVertex2i, (GLint x, GLint y), (x, y))
WRAP_VOID(glVertex3fv, (const GLfloat *v), (v))
WRAP_VOID(glRasterPos2i, (GLint x, GLint y), (x, y))

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
    MAP(glVertex2f);
    MAP(glVertex2i);
    MAP(glVertex3fv);
    MAP(glRasterPos2i);
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

    // CRITICAL: Return NULL if we haven't wrapped it!
    // If we return the real driver address (STDCALL), the Linux binary (CDECL)
    // will crash due to stack corruption (mismatched cleanup).
    return NULL;
}
