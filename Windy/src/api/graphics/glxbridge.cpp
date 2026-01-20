#include "GLXBridge.h"
#include <vector>
#include <string>
#include <malloc.h>
#include <cstring>
#include <cmath>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h> 

extern void (*g_glutKeyboardFunc)(unsigned char key, int x, int y);

// ==========================================================================
//   Global State
// ==========================================================================
SDL_Window* g_sdlWindow = nullptr;
SDL_GLContext g_sdlContext = nullptr;

// GLUT Callbacks
static void (*g_glutDisplayFunc)(void) = nullptr;
static void (*g_glutIdleFunc)(void) = nullptr;

// GLUT State
int g_windowWidth = 1360;  // Default
int g_windowHeight = 768;  // Default
int g_windowX = -1;
int g_windowY = -1;
bool g_redisplayNeeded = true;

// Constants for GLU/GLUT
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==========================================================================
//   Helper Functions
// ==========================================================================

void EnsureSDLInitialized() {
    static bool initialized = false;
    if (!initialized) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            printf("[GLX] SDL_Init failed: %s\n", SDL_GetError());
        }
        else {
            printf("[GLX] SDL3 Initialized successfully.\n");
        }
        initialized = true;
    }
}

void CreateSDLWindowIfNeeded() {
    if (g_sdlWindow) return;

    EnsureSDLInitialized();

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    g_sdlWindow = SDL_CreateWindow("Windy - Sega Lindbergh loader for Windows", g_windowWidth, g_windowHeight, flags);
    if (!g_sdlWindow) {
        printf("[GLX] Failed to create SDL Window: %s\n", SDL_GetError());
    }
    else {
        printf("[GLX] SDL Window created (%dx%d).\n", g_windowWidth, g_windowHeight);
        if (g_windowX != -1 && g_windowY != -1) {
            SDL_SetWindowPosition(g_sdlWindow, g_windowX, g_windowY);
        }
        else {
            SDL_SetWindowPosition(g_sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }
}

// ==========================================================================
//   GLX Implementation
// ==========================================================================

struct DummyVisual {
    void* ext_data;
    unsigned long visualid;
    int class_type;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb;
    int map_entries;
};

struct XVisualInfo {
    void* visual;
    unsigned long visualid;
    int screen;
    int depth;
    int class_type;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
};

void* GLXBridge::ChooseVisual(void* dpy, int screen, int* attribList) {
    EnsureSDLInitialized();
    DummyVisual* dv = (DummyVisual*)calloc(1, sizeof(DummyVisual));
    XVisualInfo* vi = (XVisualInfo*)calloc(1, sizeof(XVisualInfo));
    if (vi) {
        vi->visual = dv;
        vi->depth = 24;
        vi->class_type = 4; // TrueColor
        vi->bits_per_rgb = 8;
    }
    return vi;
}

void* GLXBridge::CreateContext(void* dpy, void* vis, void* shareList, bool direct) {
    CreateSDLWindowIfNeeded();
    if (!g_sdlWindow) return NULL;

    SDL_GLContext ctx = SDL_GL_CreateContext(g_sdlWindow);
    if (!g_sdlContext) g_sdlContext = ctx;

    // VSync off by default for performance
    SDL_GL_SetSwapInterval(0);

    return (void*)ctx;
}

void GLXBridge::DestroyContext(void* dpy, void* ctx) {
    SDL_GL_DestroyContext((SDL_GLContext)ctx);
}

int GLXBridge::MakeCurrent(void* dpy, unsigned long drawable, void* ctx) {
    if (!g_sdlWindow) CreateSDLWindowIfNeeded();
    if (!ctx) return SDL_GL_MakeCurrent(g_sdlWindow, NULL) ? 1 : 0;
    return SDL_GL_MakeCurrent(g_sdlWindow, (SDL_GLContext)ctx) ? 1 : 0;
}

void GLXBridge::SwapBuffers(void* dpy, unsigned long drawable) {
    if (g_sdlWindow) {
        SDL_GL_SwapWindow(g_sdlWindow);
        // Process minimal events to keep window responsive
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) exit(0);
        }
    }
}

int GLXBridge::SwapInterval(int interval) { return SDL_GL_SetSwapInterval(interval); }
void* GLXBridge::GetProcAddress(const char* procName) { return (void*)SDL_GL_GetProcAddress(procName); }
const char* GLXBridge::QueryExtensionsString(void* dpy, int screen) { return ""; }
const char* GLXBridge::QueryServerString(void* dpy, int screen, int name) { return ""; }
const char* GLXBridge::GetClientString(void* dpy, int name) { return ""; }
void* GLXBridge::GetCurrentDisplay() { return NULL; }
void* GLXBridge::GetCurrentContext() { return (void*)SDL_GL_GetCurrentContext(); }
unsigned long GLXBridge::GetCurrentDrawable() { return 1; }
int GLXBridge::IsDirect(void* dpy, void* ctx) { return 1; }
void* GLXBridge::ChooseFBConfigSGIX(void* dpy, int screen, int* attrib_list, int* nelements) {
    if (nelements) *nelements = 1;
    static void* cfg[] = { (void*)1, NULL };
    return cfg;
}
int GLXBridge::GetFBConfigAttribSGIX(void* dpy, void* config, int attribute, int* value) { if (value)*value = 0; return 0; }
void* GLXBridge::CreateContextWithConfigSGIX(void* dpy, void* config, int render_type, void* share_list, bool direct) { return CreateContext(dpy, NULL, share_list, direct); }
void* GLXBridge::CreateGLXPbufferSGIX(void* dpy, void* config, unsigned int width, unsigned int height, int* attrib_list) { return (void*)0xDEAD; }
void GLXBridge::DestroyGLXPbufferSGIX(void* dpy, void* pbuf) {}

// ==========================================================================
//   GLUT Implementation
// ==========================================================================

void GLXBridge::glutInit(int* argcp, char** argv) {
    printf("[GLUT] glutInit called.\n");
    EnsureSDLInitialized();
}

void GLXBridge::glutInitDisplayMode(unsigned int mode) {
    EnsureSDLInitialized();
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

void GLXBridge::glutInitWindowSize(int width, int height) {
    g_windowWidth = width;
    g_windowHeight = height;
}

void GLXBridge::glutInitWindowPosition(int x, int y) {
    g_windowX = x;
    g_windowY = y;
}

int GLXBridge::glutEnterGameMode() {
    printf("[GLUT] glutEnterGameMode called.\n");
    CreateSDLWindowIfNeeded();
    // SDL_SetWindowFullscreen(g_sdlWindow, SDL_WINDOW_FULLSCREEN); // Optional
    if (!g_sdlContext) g_sdlContext = SDL_GL_CreateContext(g_sdlWindow);
    SDL_GL_MakeCurrent(g_sdlWindow, g_sdlContext);
    return 1;
}

void GLXBridge::glutLeaveGameMode() {
    if (g_sdlWindow) SDL_SetWindowFullscreen(g_sdlWindow, 0);
}

void GLXBridge::glutMainLoop() {
    printf("[GLUT] Starting glutMainLoop...\n");
    CreateSDLWindowIfNeeded();
    if (!g_sdlContext) {
        g_sdlContext = SDL_GL_CreateContext(g_sdlWindow);
        SDL_GL_MakeCurrent(g_sdlWindow, g_sdlContext);
    }

    bool running = true;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; exit(0); }
            else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (g_glutKeyboardFunc) {
                    float mx, my;
                    SDL_GetMouseState(&mx, &my);
                    g_glutKeyboardFunc((unsigned char)e.key.key, (int)mx, (int)my);
                }
            }
        }
        if (g_glutIdleFunc) g_glutIdleFunc();
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
void GLXBridge::glutSwapBuffers() { if (g_sdlWindow) SDL_GL_SwapWindow(g_sdlWindow); }
int GLXBridge::glutGet(int state) { return (state == 700) ? (int)SDL_GetTicks() : 0; }
void GLXBridge::glutSetCursor(int cursor) {}
void GLXBridge::glutGameModeString(const char* string) { printf("[GLUT] GameModeString: %s\n", string); }
void GLXBridge::glutBitmapCharacter(void* font, int character) {}
int GLXBridge::glutBitmapWidth(void* font, int character) { return 8; }

// GLU
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
    double m[16] = { s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, 0, 0, 0, 1 };
    glMultMatrixd(m);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}