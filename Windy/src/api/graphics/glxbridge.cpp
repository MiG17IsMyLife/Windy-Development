#define _CRT_SECURE_NO_WARNINGS

#include "GLXBridge.h"
#include "X11Bridge.h"
#include "../../core/log.h"

#include <vector>
#include <string>
#include <malloc.h>
#include <cstring>
#include <cmath>
#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// SDL3 & OpenGL
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h> 

// GLUT Constants (Minimal)
#define GLUT_RGBA           0x0000
#define GLUT_DOUBLE         0x0002
#define GLUT_DEPTH          0x0010
#define GLUT_STENCIL        0x0020

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// External Global State
extern SDL_Window* g_sdlWindow;
extern int g_windowWidth;
extern int g_windowHeight;

// GLUT Callbacks
static void (*g_glutDisplayFunc)(void) = nullptr;
static void (*g_glutIdleFunc)(void) = nullptr;
static void (*g_glutKeyboardFunc)(unsigned char key, int x, int y) = nullptr;
static bool g_redisplayNeeded = true;

// --------------------------------------------------------------------------
// Internal Structure Definitions
// --------------------------------------------------------------------------
struct __GLXFBConfigRec {
    int dummy_attr;
};

// ==========================================================================
//   Helper Functions
// ==========================================================================

static void EnsureSDLInitialized() {
    static bool initialized = false;
    if (!initialized) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            log_error("GLXBridge: SDL_Init failed: %s", SDL_GetError());
        }
        initialized = true;
    }
}

// ==========================================================================
//   GLX Implementation
// ==========================================================================

XVisualInfo* GLXBridge::ChooseVisual(Display* dpy, int screen, int* attribList) {
    log_trace("glXChooseVisual(screen=%d)", screen);
    EnsureSDLInitialized();

    XVisualInfo* vi = (XVisualInfo*)calloc(1, sizeof(XVisualInfo));
    if (vi) {
        vi->visual = (void*)1;
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

GLXContext GLXBridge::CreateContext(Display* dpy, XVisualInfo* vis, GLXContext shareList, bool direct) {
    log_debug("glXCreateContext(vis=%p, share=%p, direct=%d)", vis, shareList, direct);

    if (!g_sdlWindow) {
        XSetWindowAttributes attr = { 0 };
        X11Bridge::XCreateWindow(dpy, 0, 0, 0, 1360, 768, 0, 24, 0, nullptr, 0, &attr);
    }

    if (!g_sdlWindow) return nullptr;

    SDL_GLContext ctx = SDL_GL_CreateContext(g_sdlWindow);
    if (!ctx) {
        log_error("glXCreateContext failed: %s", SDL_GetError());
        return nullptr;
    }

    // VSync off by default
    SDL_GL_SetSwapInterval(0);

    log_info("Created OpenGL Context: %p", ctx);
    return (GLXContext)ctx;
}

void GLXBridge::DestroyContext(Display* dpy, GLXContext ctx) {
    log_debug("glXDestroyContext(%p)", ctx);
    if (ctx) SDL_GL_DestroyContext((SDL_GLContext)ctx);
}

int GLXBridge::MakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx) {
    SDL_Window* win = X11Bridge::GetSDLWindow(drawable);
    if (!win) win = g_sdlWindow; // Fallback

    if (!win) return 0; // Window not found

    if (!ctx) {
        return SDL_GL_MakeCurrent(win, NULL) ? 1 : 0;
    }

    if (SDL_GL_MakeCurrent(win, (SDL_GLContext)ctx)) {
        return 1;
    }
    else {
        log_error("glXMakeCurrent failed: %s", SDL_GetError());
        return 0;
    }
}

void GLXBridge::SwapBuffers(Display* dpy, GLXDrawable drawable) {
    SDL_Window* win = X11Bridge::GetSDLWindow(drawable);
    if (win) {
        SDL_GL_SwapWindow(win);
    }
}

int GLXBridge::SwapInterval(int interval) {
    return SDL_GL_SetSwapInterval(interval) ? 0 : 1;
}

void* GLXBridge::GetProcAddress(const char* procName) {
    return (void*)SDL_GL_GetProcAddress(procName);
}

// --- Info / Query ---

const char* GLXBridge::QueryExtensionsString(Display* dpy, int screen) {
    return "GLX_SGIX_fbconfig GLX_SGIX_pbuffer GLX_ARB_create_context";
}
const char* GLXBridge::QueryServerString(Display* dpy, int screen, int name) { return "Sega Lindbergh Emulator"; }
const char* GLXBridge::GetClientString(Display* dpy, int name) { return "Windy GLX Bridge"; }
Display* GLXBridge::GetCurrentDisplay() { return (Display*)0x1111; }
GLXContext GLXBridge::GetCurrentContext() { return (GLXContext)SDL_GL_GetCurrentContext(); }
GLXDrawable GLXBridge::GetCurrentDrawable() {
    return X11Bridge::GetXWindowFromSDL(SDL_GL_GetCurrentWindow());
}
int GLXBridge::IsDirect(Display* dpy, GLXContext ctx) { return 1; }
int GLXBridge::GetConfig(Display* dpy, XVisualInfo* vis, int attrib, int* value) {
    if (value) *value = 0;
    return 0;
}

// --- SGIX Extensions ---

GLXFBConfig* GLXBridge::ChooseFBConfigSGIX(Display* dpy, int screen, int* attrib_list, int* nelements) {
    static struct __GLXFBConfigRec dummyConfig;
    static GLXFBConfig configs[] = { &dummyConfig, nullptr };
    if (nelements) *nelements = 1;
    return configs;
}

int GLXBridge::GetFBConfigAttribSGIX(Display* dpy, GLXFBConfig config, int attribute, int* value) {
    if (value) *value = 0;
    return 0;
}

XVisualInfo* GLXBridge::GetVisualFromFBConfig(Display* dpy, GLXFBConfig config) {
    return ChooseVisual(dpy, 0, nullptr);
}

GLXContext GLXBridge::CreateContextWithConfigSGIX(Display* dpy, GLXFBConfig config, int render_type, GLXContext share_list, bool direct) {
    return CreateContext(dpy, nullptr, share_list, direct);
}

GLXPbuffer GLXBridge::CreateGLXPbufferSGIX(Display* dpy, GLXFBConfig config, unsigned int width, unsigned int height, int* attrib_list) {
    return 10001; // Fake ID
}

void GLXBridge::DestroyGLXPbufferSGIX(Display* dpy, GLXPbuffer pbuf) {}

// ==========================================================================
//   GLUT Implementation
// ==========================================================================

void GLXBridge::glutInit(int* argcp, char** argv) {
    log_info("glutInit");
    EnsureSDLInitialized();
}

void GLXBridge::glutInitDisplayMode(unsigned int mode) {
    log_debug("glutInitDisplayMode(0x%X)", mode);
    EnsureSDLInitialized();

    if (mode & GLUT_DOUBLE) SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    else SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

    if (mode & GLUT_DEPTH) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    if (mode & GLUT_STENCIL) SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    if (mode & GLUT_RGBA) { /* Default */ }
}

void GLXBridge::glutInitWindowSize(int width, int height) {
    g_windowWidth = width;
    g_windowHeight = height;
}

void GLXBridge::glutInitWindowPosition(int x, int y) {
    // g_windowX = x; 
    // g_windowY = y;
}

int GLXBridge::glutCreateWindow(const char* title) {
    log_info("glutCreateWindow: %s", title);
    XSetWindowAttributes attr = { 0 };
    Window win = X11Bridge::XCreateWindow(nullptr, 0, 0, 0, g_windowWidth, g_windowHeight, 0, 24, 0, nullptr, 0, &attr);
    X11Bridge::StoreName(nullptr, win, title);
    return (int)win;
}

int GLXBridge::glutEnterGameMode() {
    log_info("glutEnterGameMode");
    glutCreateWindow("Windy - SEGA Lindbergh Emulator for Windows");
    return 1;
}

void GLXBridge::glutLeaveGameMode() {
    if (g_sdlWindow) SDL_SetWindowFullscreen(g_sdlWindow, 0);
}

void GLXBridge::glutMainLoop() {
    log_info(">>> glutMainLoop <<<");

    if (!SDL_GL_GetCurrentContext() && g_sdlWindow) {
        SDL_GLContext ctx = SDL_GL_CreateContext(g_sdlWindow);
        SDL_GL_MakeCurrent(g_sdlWindow, ctx);
    }

    bool running = true;
    XEvent ev;
    Display* dpy = (Display*)0x1111; // Dummy

    while (running) {
        while (X11Bridge::Pending(dpy)) {
            X11Bridge::NextEvent(dpy, &ev);

            // 将来的にここでGLUTコールバックを呼ぶ
            // if (ev.type == KeyPress && g_glutKeyboardFunc) ...
        }

        // Idle
        if (g_glutIdleFunc) g_glutIdleFunc();

        // Redisplay
        if (g_redisplayNeeded && g_glutDisplayFunc) {
            g_glutDisplayFunc();
            g_redisplayNeeded = false;
        }

        SDL_Delay(1);

    }
}

void GLXBridge::glutDisplayFunc(void (*func)(void)) { g_glutDisplayFunc = func; }
void GLXBridge::glutIdleFunc(void (*func)(void)) { g_glutIdleFunc = func; }
void GLXBridge::glutPostRedisplay() { g_redisplayNeeded = true; }

void GLXBridge::glutSwapBuffers() {
    if (g_sdlWindow) SDL_GL_SwapWindow(g_sdlWindow);
}

int GLXBridge::glutGet(int state) { return 0; }
void GLXBridge::glutSetCursor(int cursor) {}
void GLXBridge::glutGameModeString(const char* string) {}
void GLXBridge::glutBitmapCharacter(void* font, int character) {}
int GLXBridge::glutBitmapWidth(void* font, int character) { return 8; }

// --- GLU ---

void GLXBridge::gluPerspective(double fovy, double aspect, double zNear, double zFar) {
    double f = 1.0 / tan(fovy * M_PI / 360.0);
    double m[16] = { 0 };
    m[0] = f / aspect; m[5] = f; m[10] = (zFar + zNear) / (zNear - zFar); m[11] = -1.0; m[14] = (2.0 * zFar * zNear) / (zNear - zFar);
    glMultMatrixd(m);
}

void GLXBridge::gluLookAt(double eyeX, double eyeY, double eyeZ, double centerX, double centerY, double centerZ, double upX, double upY, double upZ) {
    double f[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    double mag = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (mag > 0) { f[0] /= mag; f[1] /= mag; f[2] /= mag; }
    double u[3] = { upX, upY, upZ };
    mag = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (mag > 0) { u[0] /= mag; u[1] /= mag; u[2] /= mag; }
    double s[3] = { f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0] };
    mag = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (mag > 0) { s[0] /= mag; s[1] /= mag; s[2] /= mag; }
    u[0] = s[1] * f[2] - s[2] * f[1]; u[1] = s[2] * f[0] - s[0] * f[2]; u[2] = s[0] * f[1] - s[1] * f[0];
    double mat[16] = { s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, 0, 0, 0, 1 };
    glMultMatrixd(mat);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}