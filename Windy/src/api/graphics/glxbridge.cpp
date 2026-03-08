#define _CRT_SECURE_NO_WARNINGS

#include "glxbridge.h"
#include "glhooks.h"
#include "sdlcalls.h"
#include "x11bridge.h"
#include "../../core/log.h"

#include <GL/glu.h>
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
#include "../../../include/LiberationMono-Regular.h"

#include "../../hardware/jvsboard.h"
#include "../../hardware/lindberghdevice.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GLX_PBUFFER_HEIGHT 0x8040
#define GLX_PBUFFER_WIDTH 0x8041
#define GLX_LARGEST_PBUFFER 0x801C

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
    printf("glXChooseVisual(screen=%d)\n", screen);
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

// GLXContext GLXBridge::CreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, bool direct)
// {
//     printf("glXCreateContext Called\n");

//     if (!SDLCalls::GetWindow())
//     {
//         XSetWindowAttributes attr = {0};
//         X11Bridge::XCreateWindow(dpy, 0, 0, 0, 1, 1, 0, 24, 0, nullptr, 0, &attr);
//         if (SDLCalls::GetWindow())
//             SDL_HideWindow(SDLCalls::GetWindow());
//     }

//     if (!SDLCalls::GetWindow())
//     {
//         log_error("Failed to create dummy window for context creation");
//         return nullptr;
//     }

//     // Handle context sharing for PBuffers
//     SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, shareList != NULL ? 1 : 0);

//     SDL_GLContext ctx = SDL_GL_CreateContext(SDLCalls::GetWindow());
//     if (!ctx)
//     {
//         printf("glXCreateContext failed: %s\n", SDL_GetError());
//         return nullptr;
//     }

//     printf("Created OpenGL Context: %p\n", ctx);
//     return (GLXContext)ctx;
// }

GLXContext GLXBridge::CreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, bool direct)
{
    log_debug("glXCreateContext(vis=%p, "
              "share=%p, direct=%d)",
              vis, shareList, direct);

    if (!SDLCalls::GetWindow())
    {
        XSetWindowAttributes attr = {};
        X11Bridge::XCreateWindow(dpy, 0, 0, 0, 1280, 768, 0, 24, 0, nullptr, 0, &attr);
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

bool GLXBridge::MakeCurrent(Display * /*dpy*/, GLXDrawable drawable, GLXContext ctx)
{
    return MakeContextCurrent(nullptr, drawable, drawable, ctx);
}

bool GLXBridge::MakeContextCurrent(Display * /*dpy*/, GLXDrawable drawable, GLXDrawable read, GLXContext ctx)
{
    // printf("glXMakeContextCurrent(draw=%p, read=%p, ctx=%p)\n", (void*)draw, (void*)read, (void*)ctx);
    if (drawable != read)
    {
        printf("Read and Draw drawables are different.\n");
    }

    SDL_Window *target_window = nullptr;

    if (drawable == 0)
    { // Unbind context
        if (SDLCalls::MakeCurrent(nullptr, nullptr) < 0)
        {
            log_error("glXMakeContextCurrent failed to unbind context: %s\n", SDL_GetError());
            return 0;
        }
        return 1;
    }
    else if (drawable == 1)
    {
        target_window = SDLCalls::GetWindow();
    }
    else
    {
        target_window = (SDL_Window *)drawable;
    }

    if (!target_window)
    {
        log_error(" drawable %p -> NULL window\n", (void *)drawable);
        return 0;
    }

    if (!SDL_GL_MakeCurrent(target_window, (SDL_GLContext)ctx))
    {
        log_error("glXMakeCurrent failed for window %p context %p: %s\n", (void *)target_window, (void *)ctx, SDL_GetError());
        return 0;
    }

    return 1;
}

static void ProcessGLXEvents()
{

    SDL_PumpEvents();

    JvsBoard *jvs = LindberghDevice::Instance().GetJvsBoard();
    if (!jvs)
    {
        static bool warned = false;
        if (!warned)
        {
            log_warn("ProcessGLXEvents: JVS Board is NULL!");
            warned = true;
        }
    }
    if (jvs)
    {

        const bool *state = SDL_GetKeyboardState(NULL);

        jvs->SetSwitch(PLAYER_1, BUTTON_SERVICE, state[SDL_SCANCODE_9]);

        jvs->SetSwitch(PLAYER_1, BUTTON_START, state[SDL_SCANCODE_1]);

        jvs->SetSwitch(SYSTEM, BUTTON_TEST, state[SDL_SCANCODE_0]);

        // jvs->SetSwitch(PLAYER_1, BUTTON_1, state[SDL_SCANCODE_Z]);
        // jvs->SetSwitch(PLAYER_1, BUTTON_2, state[SDL_SCANCODE_X]);

        static bool oldCoin = false;
        bool coin = state[SDL_SCANCODE_5];
        if (coin && !oldCoin)
        {
            jvs->IncrementCoin(PLAYER_1, 1);
        }
        oldCoin = coin;

        float mx, my;
        uint32_t mouseMask = SDL_GetMouseState(&mx, &my);

        jvs->SetSwitch(PLAYER_1, BUTTON_1, (mouseMask & SDL_BUTTON_LMASK) != 0);
        jvs->SetSwitch(PLAYER_1, BUTTON_2, (mouseMask & SDL_BUTTON_RMASK) != 0);

        if (getConfig()->width > 0 && getConfig()->height > 0)
        {
            int a1 = (int)((mx / (float)getConfig()->width) * 1023.0f);
            if (a1 < 0)
                a1 = 0;
            if (a1 > 1023)
                a1 = 1023;

            int a2 = (int)((my / (float)getConfig()->height) * 1023.0f);
            if (a2 < 0)
                a2 = 0;
            if (a2 > 1023)
                a2 = 1023;

            jvs->SetAnalogue(ANALOGUE_1, a1);
            jvs->SetAnalogue(ANALOGUE_2, a2);
        }
    }
}

void GLXBridge::SwapBuffers(Display * /*dpy*/, GLXDrawable drawable)
{
    SDL_Window *win = X11Bridge::GetSDLWindow(drawable);
    if (win)
    {
        SDL_GL_SwapWindow(win);
    }
    // ProcessGLXEvents();
    SDLCalls::PollEvents();
}

int GLXBridge::SwapInterval(int interval)
{
    return SDL_GL_SetSwapInterval(interval) ? 0 : 1;
}

bool GLXBridge::QueryVersion(Display * /*dpy*/, int *major, int *minor)
{
    if (major)
        *major = 1;
    if (minor)
        *minor = 4;
    return true;
}

void *GLXBridge::GetProcAddress(const char *procName)
{
    void *proc = nullptr;
    // Should call the function in glhooks.cpp
    if (getConfig()->gameId == INITIALD_5_JAP_REVA)
    {
        if ((strcmp((const char *)procName, "glProgramEnvParameters4fvNVARB\0") == 0) ||
            (strcmp((const char *)procName, "glProgramEnvParameters4fvARB\0") == 0))
        {
            if (1)
            {
                printf("glProgramEnvParameters4fvNVARB Intercepted.\n");
            }
            proc = GLHooks::GetProcAddress("glProgramEnvParameters4fvEXT");
            if (!proc)
            {
                printf("glProgramEnvParameters4fvEXT not found\n");
            }
            else
            {
                printf("glProgramEnvParameters4fvEXT found\n");
            }
            return proc;
        }
        else if ((strcmp((const char *)procName, "glProgramLocalParameters4fvNVARB\0") == 0) ||
                 (strcmp((const char *)procName, "glProgramLocalParameters4fvARB\0") == 0))
        {
            if (1)
            {
                printf("glProgramLocalParameters4fvNVARB Intercepted.\n");
            }
            proc = GLHooks::GetProcAddress("glProgramLocalParameters4fvEXT");
            if (!proc)
            {
                printf("glProgramLocalParameters4fvEXT not found\n");
            }
            else
            {
                printf("glProgramLocalParameters4fvEXT found\n");
            }
            return proc;
        }
        else if (strcmp((const char *)procName, "glProgramParameters4fvNVARB\0") == 0)
        {
            if (1)
            {
                printf("glProgramParameters4fvNVARB Intercepted.\n");
            }
            proc = GLHooks::GetProcAddress("glProgramLocalParameters4fvEXT");
            if (!proc)
            {
                printf("glProgramLocalParameters4fvEXT not found\n");
            }
            else
            {
                printf("glProgramLocalParameters4fvEXT found\n");
            }
            return proc;
        }
    }
    proc = GLHooks::GetProcAddress(procName);
    if (proc)
    {

        // void *retAddr = __builtin_return_address(0);
        // if ((uintptr_t)retAddr >= (uintptr_t)proc && (uintptr_t)retAddr < (uintptr_t)proc + 4096)
        // {
        //     return (void *)SDL_GL_GetProcAddress(procName);
        // }
        // return proc;
    }

    log_error("GLXBridge::GetProcAddress: Missing wrapper for '%s'", procName);
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

static char g_dummy_display_mem[1024] = {0};
Display *GLXBridge::GetCurrentDisplay()
{
    // printf("glXGetCurrentDisplay called\n");
    return (Display *)&g_dummy_display_mem;
}

GLXContext GLXBridge::GetCurrentContext()
{
    return (GLXContext)SDLCalls::GetContext();
}

GLXDrawable GLXBridge::GetCurrentDrawable()
{
    return (GLXDrawable)SDLCalls::GetWindow();
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

GLXFBConfig *GLXBridge::ChooseFBConfig(Display * /*dpy*/, int /*screen*/, int * /*attrib_list*/, int *nelements)
{
    printf("glXChooseFBConfig\n");
    static struct __GLXFBConfigRec dummyConfig;

    GLXFBConfig *configs = (GLXFBConfig *)malloc(sizeof(GLXFBConfig) * 2);
    if (configs)
    {
        configs[0] = &dummyConfig;
        configs[1] = nullptr;
    }
    if (nelements)
        *nelements = 1;
    return configs;
}

// --- SGIX Extensions ---

GLXFBConfig *GLXBridge::ChooseFBConfigSGIX(Display * /*dpy*/, int /*screen*/, int * /*attrib_list*/, int *nelements)
{
    printf("glXChooseFBConfigSGIX Called\n");
    return ChooseFBConfig(nullptr, 0, nullptr, nelements);
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

GLXPbuffer GLXBridge::CreatePbuffer(Display * /*dpy*/, GLXFBConfig /*config*/, const int *attrib_list)
{
    printf("glXCreatePbuffer Called\n");
    int width = 0;
    int height = 0;

    if (attrib_list)
    {
        for (int i = 0; attrib_list[i] != 0; i += 2)
        {
            if (attrib_list[i] == GLX_PBUFFER_WIDTH)
            {
                width = attrib_list[i + 1];
            }
            if (attrib_list[i] == GLX_PBUFFER_HEIGHT)
            {
                height = attrib_list[i + 1];
            }
        }
    }

    if (width == 0)
        width = 512;
    if (height == 0)
        height = 512;

    printf("Creating fake pbuffer (hidden window) of size %dx%d\n", width, height);

    SDL_Window *pbuf_window =
        SDL_CreateWindow("Fake Pbuffer", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS);

    if (!pbuf_window)
    {
        printf("Failed to create hidden window for Pbuffer: %s\n", SDL_GetError());
        return 0;
    }

    return (GLXPbuffer)pbuf_window;
}

void GLXBridge::DestroyPbuffer(Display * /*dpy*/, GLXPbuffer pbuf)
{
    printf("glXDestroyPbuffer(%p)\n", (void *)pbuf);
    if (pbuf)
    {
        SDL_DestroyWindow((SDL_Window *)pbuf);
    }
}

GLXContext GLXBridge::CreateNewContext(Display * /*dpy*/, GLXFBConfig config, int /*render_type*/, GLXContext share_list, bool direct)
{
    printf("glXCreateNewContext(config=%p, share=%p, direct=%d)\n", config, share_list, direct);

    if (!SDLCalls::GetWindow())
    {
        log_error("No window available for context creation");
        return nullptr;
    }

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, share_list != NULL ? 1 : 0);

    SDL_GLContext ctx = SDL_GL_CreateContext(SDLCalls::GetWindow());
    if (!ctx)
    {
        log_error("Failed to create GL context: %s", SDL_GetError());
        return nullptr;
    }
    log_info("Created GL context: %p", ctx);
    return (GLXContext)ctx;
}

bool GLXBridge::QueryExtension(Display * /*dpy*/, const char *extList)
{
    printf("glXQueryExtension(\"%s\") called\n", extList);

    return true;
}

// ==========================================================================
//   GLU Implementation
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

void GLXBridge::gluOrtho2D(double left, double right, double bottom, double top)
{
    glOrtho(left, right, bottom, top, -1.0, 1.0);
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

char glubuffer[64];
const char *GLXBridge::gluErrorString(unsigned int error)
{
    switch (error)
    {
        case GLU_INVALID_ENUM:
            return "GLU_INVALID_ENUM";
        case GLU_INVALID_VALUE:
            return "GLU_INVALID_VALUE";
        case GLU_OUT_OF_MEMORY:
            return "GLU_OUT_OF_MEMORY";
        default:
            sprintf(glubuffer, "Unknown GLU error: %d", error);
            return glubuffer;
    }
}

int GLXBridge::GetVideoSyncSGI(unsigned int *count)
{
    static unsigned int frameCount = 0;
    *count = (frameCount++) / 2;
    return 0;
}

int GLXBridge::GetRefreshRateSGI(unsigned int *rate)
{
    *rate = 60;
    return 0;
}

int GLXBridge::SwapIntervalSGI(int /*interval*/)
{
    return 0;
}