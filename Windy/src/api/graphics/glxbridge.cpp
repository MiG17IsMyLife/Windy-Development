#include "GLXBridge.h"
#include "X11Bridge.h"
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <GL/gl.h>
#include <malloc.h>

// WGL Definitions
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTPROC) (HDC hdc);
typedef BOOL(WINAPI* PFNWGLDELETECONTEXTPROC) (HGLRC hglrc);
typedef BOOL(WINAPI* PFNWGLMAKECURRENTPROC) (HDC hdc, HGLRC hglrc);
typedef PROC(WINAPI* PFNWGLGETPROCADDRESSPROC) (LPCSTR lpszProc);

static HGLRC g_hRC = NULL;
static HDC g_hDC = NULL;

// --------------------------------------------------------------------------
// Stubs for unresolved OpenGL functions
// --------------------------------------------------------------------------
static void WINAPI GenericGLStub() {
    std::cout << "[GLX] WARNING: Game called an unresolved OpenGL extension!" << std::endl;
}

// --------------------------------------------------------------------------
// Helper: Lazy Initialization for WGL Extensions
// --------------------------------------------------------------------------
static void GrabExtensionFuncs() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "WindyDummyGL";
    RegisterClassA(&wc);

    HWND dummyW = CreateWindowA("WindyDummyGL", "", 0, 0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL);
    if (!dummyW) return;

    HDC dummyDC = GetDC(dummyW);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    int fmt = ChoosePixelFormat(dummyDC, &pfd);
    SetPixelFormat(dummyDC, fmt, &pfd);

    HGLRC dummyRC = wglCreateContext(dummyDC);
    if (dummyRC) {
        wglMakeCurrent(dummyDC, dummyRC);
    }
}

// --------------------------------------------------------------------------

// XVisualInfo for allocation
struct XVisualInfo {
    void* visual; // DummyVisual*
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

struct DummyVisual {
    void* ext_data;
    unsigned long visualid;
    int class_type;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb;
    int map_entries;
};

void* GLXBridge::ChooseVisual(void* dpy, int screen, int* attribList) {
    std::cout << "[GLX] glXChooseVisual called" << std::endl;

    DummyVisual* dv = (DummyVisual*)_aligned_malloc(sizeof(DummyVisual), 16);
    if (dv) {
        memset(dv, 0, sizeof(DummyVisual));
        dv->visualid = 0x21;
        dv->class_type = 4;
        dv->bits_per_rgb = 8;
        dv->map_entries = 256;
    }

    XVisualInfo* vi = (XVisualInfo*)_aligned_malloc(sizeof(XVisualInfo), 16);
    if (vi) {
        memset(vi, 0, sizeof(XVisualInfo));
        vi->visual = dv;
        vi->visualid = 0x21;
        vi->screen = 0;
        vi->depth = 24;
        vi->class_type = 4;
        vi->red_mask = 0xFF0000;
        vi->green_mask = 0x00FF00;
        vi->blue_mask = 0x0000FF;
        vi->bits_per_rgb = 8;
        vi->colormap_size = 256;
    }

    return vi;
}

void* GLXBridge::CreateContext(void* dpy, void* vis, void* shareList, bool direct) {
    std::cout << "[GLX] glXCreateContext called" << std::endl;

    HWND hwnd = X11Bridge::GetHwnd();
    if (!hwnd) {
        std::cerr << "  -> ERROR: No Window created yet!" << std::endl;
        return NULL;
    }

    g_hDC = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(g_hDC, &pfd);
    SetPixelFormat(g_hDC, format, &pfd);

    g_hRC = wglCreateContext(g_hDC);

    if (g_hRC) {
        std::cout << "  -> Created WGL Context: " << g_hRC << std::endl;
        if (shareList) {
            wglShareLists((HGLRC)shareList, g_hRC); 
        }
    }
    else {
        std::cerr << "  -> FAILED to create WGL Context! Error: " << GetLastError() << std::endl;
    }

    return (void*)g_hRC;
}

void GLXBridge::DestroyContext(void* dpy, void* ctx) {
    std::cout << "[GLX] glXDestroyContext called" << std::endl;
    if (ctx == g_hRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext((HGLRC)ctx);
        g_hRC = NULL;
    }
}

int GLXBridge::MakeCurrent(void* dpy, unsigned long drawable, void* ctx) {
    std::cout << "[GLX] glXMakeCurrent called (Ctx: " << ctx << ")" << std::endl;

    if (!ctx) return wglMakeCurrent(NULL, NULL);

    if (!wglMakeCurrent(g_hDC, (HGLRC)ctx)) {
        std::cerr << "  -> wglMakeCurrent FAILED! Error: " << GetLastError() << std::endl;
        return 0;
    }
    return 1;
}

void GLXBridge::SwapBuffers(void* dpy, unsigned long drawable) {
    static int fc = 0;
    if ((fc++ % 60) == 0) std::cout << "[GLX] glXSwapBuffers (Running...)" << std::endl;

    if (g_hDC) ::SwapBuffers(g_hDC);
}

int GLXBridge::SwapInterval(int interval) {
    return 0;
}

int GLXBridge::IsDirect(void* dpy, void* ctx) {
    std::cout << "[GLX] glXIsDirect called" << std::endl;
    return 1;
}

void* GLXBridge::GetProcAddress(const char* procName) {

    PROC addr = wglGetProcAddress(procName);

    if (!addr) {
        GrabExtensionFuncs();
        addr = wglGetProcAddress(procName);
    }

    if (!addr) {
        static HMODULE hOpenGL = LoadLibraryA("opengl32.dll");
        FARPROC faddr = ::GetProcAddress(hOpenGL, procName);
        if (faddr) return (void*)faddr;
    }

    if (!addr) {
        std::cout << "[GLX] Warning: Failed to resolve " << procName << ", returning stub." << std::endl;
        return (void*)&GenericGLStub;
    }

    return (void*)addr;
}

const char* GLXBridge::QueryExtensionsString(void* dpy, int screen) {
    return "GLX_ARB_create_context GLX_ARB_create_context_profile GLX_EXT_swap_control";
}
const char* GLXBridge::QueryServerString(void* dpy, int screen, int name) { return "Windy GLX Server"; }
const char* GLXBridge::GetClientString(void* dpy, int name) { return "Windy GLX Client"; }

void* GLXBridge::GetCurrentDisplay() { return NULL; }
void* GLXBridge::GetCurrentContext() { return (void*)g_hRC; }
unsigned long GLXBridge::GetCurrentDrawable() { return 0; }

void* GLXBridge::ChooseFBConfigSGIX(void* dpy, int screen, int* attrib_list, int* nelements) {
    std::cout << "[GLX] glXChooseFBConfigSGIX called" << std::endl;
    if (nelements) *nelements = 1;
    static void* dummyConfig = (void*)0x1234;
    static void* configs[] = { dummyConfig, NULL };
    return configs;
}

int GLXBridge::GetFBConfigAttribSGIX(void* dpy, void* config, int attribute, int* value) {
    if (value) *value = 0;
    return 0;
}

void* GLXBridge::CreateContextWithConfigSGIX(void* dpy, void* config, int render_type, void* share_list, bool direct) {
    std::cout << "[GLX] glXCreateContextWithConfigSGIX called" << std::endl;
    return CreateContext(dpy, NULL, share_list, direct);
}

void* GLXBridge::CreateGLXPbufferSGIX(void* dpy, void* config, unsigned int width, unsigned int height, int* attrib_list) { return (void*)0x9999; }
void GLXBridge::DestroyGLXPbufferSGIX(void* dpy, void* pbuf) {}