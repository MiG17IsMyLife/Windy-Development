
#include "glxbridge.h"
#include "sdlcalls.h"
#include "x11bridge.h"
#include "../../core/log.h"

#include <SDL3/SDL_scancode.h>
#include <malloc.h>
#include <cstring>
#include <cmath>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// SDL3 & OpenGL
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../core/config.h"
#include "../../../include/LiberationMono-Regular.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --------------------------------------------------------------------------
// Internal Structure Definitions
// --------------------------------------------------------------------------
struct __GLXFBConfigRec
{
    int dummy_attr;
};

// ==========================================================================
//   GLX Implementation
// ==========================================================================

XVisualInfo *GLXBridge::ChooseVisual(Display * /*dpy*/, int screen, int * /*attribList*/)
{
    log_trace("glXChooseVisual(screen=%d)", screen);
    SDLCalls::Init();

    XVisualInfo *vi = (XVisualInfo *)calloc(1, sizeof(XVisualInfo));
    if (vi)
    {
        vi->visual = (void *)1;
        vi->visualid = 1;
        vi->screen = screen;
        vi->depth = 24;
        vi->class_type = 4; // TrueColor
        vi->bits_per_rgb = 8;
        vi->red_mask = 0xFF0000;
        vi->green_mask = 0x00FF00;
        vi->blue_mask = 0x0000FF;
    }
    return vi;
}

GLXContext GLXBridge::CreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, bool direct)
{
    log_debug("glXCreateContext(vis=%p, "
              "share=%p, direct=%d)",
              vis, shareList, direct);

    if (!SDLCalls::GetWindow())
    {
        XSetWindowAttributes attr = {};
        X11Bridge::XCreateWindow(dpy, 0, 0, 0, 1360, 768, 0, 24, 0, nullptr, 0, &attr);
    }

    if (!SDLCalls::GetWindow())
        return nullptr;

    // If SDLCalls already manages the context, we return it.
    if (!SDLCalls::GetContext())
    {
        // If context doesn't exist, we might be in trouble if we expect SDLCalls to handle it.
        // For now, logging error.
        log_error("glXCreateContext: SDL Context not available from SDLCalls.");
        return nullptr;
    }

    // VSync off by default
    SDL_GL_SetSwapInterval(0);

    log_info("Created OpenGL Context: %p", SDLCalls::GetContext());
    return (GLXContext)SDLCalls::GetContext();
}

void GLXBridge::DestroyContext(Display * /*dpy*/, GLXContext ctx)
{
    log_debug("glXDestroyContext(%p)", ctx);
    if (ctx)
        SDL_GL_DestroyContext(SDLCalls::GetContext());
}

int GLXBridge::MakeCurrent(Display * /*dpy*/, GLXDrawable drawable, GLXContext ctx)
{
    SDL_Window *win = X11Bridge::GetSDLWindow(drawable);
    if (!win)
        win = SDLCalls::GetWindow(); // Fallback

    if (!win)
        return 0; // Window not found

    if (!ctx)
    {
        return SDL_GL_MakeCurrent(win, NULL) ? 1 : 0;
    }

    if (SDL_GL_MakeCurrent(win, SDLCalls::GetContext()))
    {
        return 1;
    }
    else
    {
        log_error("glXMakeCurrent failed: %s", SDL_GetError());
        return 0;
    }
}

void GLXBridge::SwapBuffers(Display * /*dpy*/, GLXDrawable drawable)
{
    SDL_Window *win = X11Bridge::GetSDLWindow(drawable);
    if (win)
    {
        SDL_GL_SwapWindow(win);
    }
}

int GLXBridge::SwapInterval(int interval)
{
    return SDL_GL_SetSwapInterval(interval) ? 0 : 1;
}

void *GLXBridge::GetProcAddress(const char *procName)
{
    return (void *)SDL_GL_GetProcAddress(procName);
}

// --- Info / Query ---

const char *GLXBridge::QueryExtensionsString(Display * /*dpy*/, int /*screen*/)
{
    return "GLX_SGIX_fbconfig GLX_SGIX_pbuffer "
           "GLX_ARB_create_context";
}
const char *GLXBridge::QueryServerString(Display * /*dpy*/, int /*screen*/, int /*name*/)
{
    return "Sega Lindbergh Emulator";
}
const char *GLXBridge::GetClientString(Display * /*dpy*/, int /*name*/)
{
    return "Windy GLX Bridge";
}
Display *GLXBridge::GetCurrentDisplay()
{
    return (Display *)0x1111;
}
GLXContext GLXBridge::GetCurrentContext()
{
    return (GLXContext)SDL_GL_GetCurrentContext();
}
GLXDrawable GLXBridge::GetCurrentDrawable()
{
    return X11Bridge::GetXWindowFromSDL(SDL_GL_GetCurrentWindow());
}
int GLXBridge::IsDirect(Display * /*dpy*/, GLXContext /*ctx*/)
{
    return 1;
}
int GLXBridge::GetConfig(Display * /*dpy*/, XVisualInfo * /*vis*/, int /*attrib*/, int *value)
{
    if (value)
        *value = 0;
    return 0;
}

// --- SGIX Extensions ---

GLXFBConfig *GLXBridge::ChooseFBConfigSGIX(Display * /*dpy*/, int /*screen*/, int * /*attrib_list*/, int *nelements)
{
    static struct __GLXFBConfigRec dummyConfig;
    static GLXFBConfig configs[] = {&dummyConfig, nullptr};
    if (nelements)
        *nelements = 1;
    return configs;
}

int GLXBridge::GetFBConfigAttribSGIX(Display * /*dpy*/, GLXFBConfig /*config*/, int /*attribute*/, int *value)
{
    if (value)
        *value = 0;
    return 0;
}

XVisualInfo *GLXBridge::GetVisualFromFBConfig(Display *dpy, GLXFBConfig /*config*/)
{
    return ChooseVisual(dpy, 0, nullptr);
}

GLXContext GLXBridge::CreateContextWithConfigSGIX(Display *dpy, GLXFBConfig /*config*/, int /*render_type*/, GLXContext share_list,
                                                  bool direct)
{
    return CreateContext(dpy, nullptr, share_list, direct);
}

GLXPbuffer GLXBridge::CreateGLXPbufferSGIX(Display * /*dpy*/, GLXFBConfig /*config*/, unsigned int /*width*/, unsigned int /*height*/,
                                           int * /*attrib_list*/)
{
    return 10001; // Fake ID
}

void GLXBridge::DestroyGLXPbufferSGIX(Display * /*dpy*/, GLXPbuffer /*pbuf*/)
{
}

// ==========================================================================
//   GLUT Implementation -> Moved to glutbridge.cpp
// ==========================================================================

void GLXBridge::gluPerspective(double fovy, double aspect, double zNear, double zFar)
{
    double f = 1.0 / tan(fovy * M_PI / 360.0);
    double m[16] = {0};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0;
    m[14] = (2.0 * zFar * zNear) / (zNear - zFar);
    glMultMatrixd(m);
}

void GLXBridge::gluLookAt(double eyeX, double eyeY, double eyeZ, double centerX, double centerY, double centerZ, double upX, double upY,
                          double upZ)
{
    double f[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
    double mag = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (mag > 0)
    {
        f[0] /= mag;
        f[1] /= mag;
        f[2] /= mag;
    }
    double u[3] = {upX, upY, upZ};
    mag = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (mag > 0)
    {
        u[0] /= mag;
        u[1] /= mag;
        u[2] /= mag;
    }
    double s[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]};
    mag = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (mag > 0)
    {
        s[0] /= mag;
        s[1] /= mag;
        s[2] /= mag;
    }
    u[0] = s[1] * f[2] - s[2] * f[1];
    u[1] = s[2] * f[0] - s[0] * f[2];
    u[2] = s[0] * f[1] - s[1] * f[0];
    double mat[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, 0, 0, 0, 1};
    glMultMatrixd(mat);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}