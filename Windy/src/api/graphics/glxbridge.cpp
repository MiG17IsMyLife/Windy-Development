#include "glxbridge.h"
#include "x11bridge.h"
#include "../../core/log.h"

#include <SDL3/SDL_scancode.h>
#include <vector>
#include <string>
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
#include "../../hardware/jvsboard.h"
#include "../../hardware/lindberghdevice.h"
#include "../../../include/LiberationMono-Regular.h"

// GLUT Constants (Minimal)
#define GLUT_RGBA 0x0000
#define GLUT_DOUBLE 0x0002
#define GLUT_DEPTH 0x0010
#define GLUT_STENCIL 0x0020

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================
// Globals
// =============================================================
extern SDL_Window *g_sdlWindow;
SDL_GLContext sdlContext;
extern int g_windowWidth;
extern int g_windowHeight;
extern uint8_t g_grp;
extern uint8_t g_enumId;

// GLUT Callbacks
static void (*g_glutDisplayFunc)(void) = nullptr;
static void (*g_glutIdleFunc)(void) = nullptr;
static bool g_redisplayNeeded = true;

// --------------------------------------------------------------------------
// Internal Structure Definitions
// --------------------------------------------------------------------------
struct __GLXFBConfigRec
{
    int dummy_attr;
};

TTF_Font *font;

// ==========================================================================
//   Helper Functions
// ==========================================================================

static void EnsureSDLInitialized()
{
    static bool initialized = false;
    if (!initialized)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            log_error("GLXBridge: SDL_Init failed: %s", SDL_GetError());
        }
        else
        {
            log_info("SDL3 initialized successfully");
        }

        if (!TTF_Init())
        {
            log_error("SDL_ttf could not initialize! SDL_ttf Error: %s\n", SDL_GetError());
        }

        // Use embedded font data to ensure it works even if file is missing
        SDL_IOStream *rw = SDL_IOFromConstMem(LiberationMonoRegular_ttf, sizeof(LiberationMonoRegular_ttf));
        float fontSize = 16.0;

        // g_font = TTF_OpenFont("LiberationMono-Regular.ttf", 16);
        font = TTF_OpenFontIO(rw, 1, fontSize);
        // g_fontRenderer = SDL_CreateRenderer(g_sdlWindow, "");
        initialized = true;
    }
}

void convertSurfaceTo1Bit(SDL_Surface *surface, uint8_t *outBitmap, int pitch)
{
    SDL_LockSurface(surface);
    for (int y = 0; y < surface->h; ++y)
    {
        uint8_t byte = 0;
        int bit = 0;
        int sdlY = surface->h - 1 - y;

        Uint8 *row = (Uint8 *)surface->pixels + sdlY * surface->pitch;

        for (int x = 0; x < surface->w; ++x)
        {
            if (row[x])
                byte |= (1 << (7 - bit));

            bit++;
            if (bit == 8 || x == surface->w - 1)
            {
                outBitmap[y * pitch + x / 8] = byte;
                byte = 0;
                bit = 0;
            }
        }
    }
    SDL_UnlockSurface(surface);
}

void sdlQuit()
{
    if (TTF_WasInit())
    {
        if (font)
        {
            TTF_CloseFont(font);
            font = NULL;
        }
        TTF_Quit();
    }

    if (sdlContext)
    {
        SDL_GL_DestroyContext(sdlContext);
        sdlContext = NULL;
    }

    if (g_sdlWindow)
    {
        SDL_DestroyWindow(g_sdlWindow);
        g_sdlWindow = NULL;
    }
    SDL_Quit();
    exit(0);
}

void pollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                sdlQuit();
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                if (event.key.scancode == SDL_SCANCODE_T)
                {
                    LindberghDevice::Instance().SetSwitch(SYSTEM, JVSInput::BUTTON_TEST, event.type == SDL_EVENT_KEY_DOWN);
                }
                if (event.key.scancode == SDL_SCANCODE_S)
                {
                    LindberghDevice::Instance().SetSwitch(PLAYER_1, JVSInput::BUTTON_SERVICE, event.type == SDL_EVENT_KEY_DOWN);
                }
            }
            default:
                break;
        }
    }
}

// ==========================================================================
//   GLX Implementation
// ==========================================================================

XVisualInfo *GLXBridge::ChooseVisual(Display * /*dpy*/, int screen, int * /*attribList*/)
{
    log_trace("glXChooseVisual(screen=%d)", screen);
    EnsureSDLInitialized();

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

    if (!g_sdlWindow)
    {
        XSetWindowAttributes attr = {};
        X11Bridge::XCreateWindow(dpy, 0, 0, 0, 1360, 768, 0, 24, 0, nullptr, 0, &attr);
    }

    if (!g_sdlWindow)
        return nullptr;

    sdlContext = SDL_GL_CreateContext(g_sdlWindow);
    if (!sdlContext)
    {
        log_error("glXCreateContext failed: %s", SDL_GetError());
        return nullptr;
    }

    // VSync off by default
    SDL_GL_SetSwapInterval(0);

    log_info("Created OpenGL Context: %p", sdlContext);
    return (GLXContext)sdlContext;
}

void GLXBridge::DestroyContext(Display * /*dpy*/, GLXContext ctx)
{
    log_debug("glXDestroyContext(%p)", ctx);
    if (ctx)
        SDL_GL_DestroyContext(sdlContext);
}

int GLXBridge::MakeCurrent(Display * /*dpy*/, GLXDrawable drawable, GLXContext ctx)
{
    SDL_Window *win = X11Bridge::GetSDLWindow(drawable);
    if (!win)
        win = g_sdlWindow; // Fallback

    if (!win)
        return 0; // Window not found

    if (!ctx)
    {
        return SDL_GL_MakeCurrent(win, NULL) ? 1 : 0;
    }

    if (SDL_GL_MakeCurrent(win, sdlContext))
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
//   GLUT Implementation
// ==========================================================================

void GLXBridge::glutInit(int * /*argcp*/, char ** /*argv*/)
{
    log_info("glutInit");
    EnsureSDLInitialized();
}

void GLXBridge::glutInitDisplayMode(unsigned int mode)
{
    log_debug("glutInitDisplayMode(0x%X)", mode);
    EnsureSDLInitialized();

    if (mode & GLUT_DOUBLE)
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    else
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

    if (mode & GLUT_DEPTH)
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    if (mode & GLUT_STENCIL)
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    if (mode & GLUT_RGBA)
    { /* Default */
    }
}

void GLXBridge::glutInitWindowSize(int width, int height)
{
    g_windowWidth = width;
    g_windowHeight = height;
    if (g_grp == GROUP_OUTRUN || g_grp == GROUP_OUTRUN_TEST)
    {
        glutCreateWindow("Windy - SEGA Lindbergh "
                         "Emulator for Windows");
        SDL_ShowWindow(g_sdlWindow);
        if (!SDL_GL_GetCurrentContext() && g_sdlWindow)
        {
            sdlContext = SDL_GL_CreateContext(g_sdlWindow);
            SDL_GL_MakeCurrent(g_sdlWindow, sdlContext);
        }
    }
}

void GLXBridge::glutInitWindowPosition(int /*x*/, int /*y*/)
{
    // g_windowX = x;
    // g_windowY = y;
}

int GLXBridge::glutCreateWindow(const char *title)
{
    log_info("glutCreateWindow: %s", title);
    XSetWindowAttributes attr = {};
    Window win = X11Bridge::XCreateWindow(nullptr, 0, 0, 0, g_windowWidth, g_windowHeight, 0, 24, 0, nullptr, 0, &attr);
    X11Bridge::StoreName(nullptr, win, title);
    return (int)win;
}

int GLXBridge::glutEnterGameMode()
{
    log_info("glutEnterGameMode");
    glutCreateWindow("Windy - SEGA Lindbergh "
                     "Emulator for Windows");
    return 1;
}

void GLXBridge::glutLeaveGameMode()
{
    if (g_sdlWindow)
        SDL_SetWindowFullscreen(g_sdlWindow, 0);
}

void GLXBridge::glutMainLoop()
{
    log_info(">>> glutMainLoop <<<");

    if (!SDL_GL_GetCurrentContext() && g_sdlWindow)
    {
        sdlContext = SDL_GL_CreateContext(g_sdlWindow);
        SDL_GL_MakeCurrent(g_sdlWindow, sdlContext);
    }

    bool running = true;
    XEvent ev;
    Display *dpy = (Display *)0x1111; // Dummy

    while (running)
    {
        pollEvents();
        while (X11Bridge::Pending(dpy))
        {
            X11Bridge::NextEvent(dpy, &ev);

            // Optionally call GLUT callbacks here
            // if (ev.type == KeyPress &&
            // g_glutKeyboardFunc) ...
        }

        // Idle
        if (g_glutIdleFunc)
            g_glutIdleFunc();

        // Redisplay
        if (g_redisplayNeeded && g_glutDisplayFunc)
        {
            g_glutDisplayFunc();
            g_redisplayNeeded = false;
        }

        SDL_Delay(1);
    }
}

void GLXBridge::glutDisplayFunc(void (*func)(void))
{
    g_glutDisplayFunc = func;
}
void GLXBridge::glutIdleFunc(void (*func)(void))
{
    g_glutIdleFunc = func;
}

void GLXBridge::glutPostRedisplay()
{
    g_redisplayNeeded = true;
}

void GLXBridge::glutSwapBuffers()
{
    if (g_grp == GROUP_OUTRUN || g_grp == GROUP_OUTRUN_TEST)
        pollEvents();

    if (g_sdlWindow)
    {
        // int w, h;
        // SDL_GetWindowSize(g_sdlWindow, &w, &h);
        SDL_GL_SwapWindow(g_sdlWindow);
    }
}

int GLXBridge::glutGet(int state)
{
    if (state == 0x66)
        return g_windowWidth;
    if (state == 0x67)
        return g_windowHeight;

    return 0;
}

void GLXBridge::glutSetCursor(int /*cursor*/)
{
}
void GLXBridge::glutGameModeString(const char * /*string*/)
{
}

int penX = 0;
int penY = 0;

void GLXBridge::glutBitmapCharacter(void * /*fontID*/, int character)
{
    GLfloat glColor[4];
    glGetFloatv(GL_CURRENT_COLOR, glColor);

    SDL_Color sdlColor;
    sdlColor.r = (Uint8)(glColor[0] * 255.0f);
    sdlColor.g = (Uint8)(glColor[1] * 255.0f);
    sdlColor.b = (Uint8)(glColor[2] * 255.0f);
    sdlColor.a = (Uint8)(glColor[3] * 255.0f);

    GLint glPos[4];
    glGetIntegerv(GL_CURRENT_RASTER_POSITION, glPos);

    if (penX != glPos[0])
        penX = glPos[0];

    penY = glPos[1];

    SDL_Surface *glyph = TTF_RenderGlyph_Solid(font, character, sdlColor);
    if (!glyph)
    {
        printf("Failed to render glyph: %s\n", SDL_GetError());
        return;
    }

    int pitch = (glyph->w + 7) / 8;
    uint8_t *bitmap = (uint8_t *)malloc(pitch * glyph->h);
    if (!bitmap)
    {
        printf("Failed to allocate memory for bitmap\n");
        SDL_DestroySurface(glyph);
        return;
    }
    convertSurfaceTo1Bit(glyph, bitmap, pitch);

    GLint swbytes, lsbfirst, rowlen, skiprows, skippix, align;

    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swbytes);
    glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlen);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippix);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);

    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBitmap(glyph->w, glyph->h, 0, 0, (float)(glyph->w), 0.0, bitmap);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION, glPos);

    glPixelStorei(GL_UNPACK_SWAP_BYTES, swbytes);
    glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippix);
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);

    free(bitmap);
    SDL_DestroySurface(glyph);
}
int GLXBridge::glutBitmapWidth(void * /*font*/, int /*character*/)
{
    return 9;
}

// --- GLU ---

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