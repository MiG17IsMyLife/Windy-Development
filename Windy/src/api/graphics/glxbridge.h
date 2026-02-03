#pragma once
#include "x11types.h"

// --------------------------------------------------------------------------
// GLX Types & Constants
// --------------------------------------------------------------------------
typedef XID GLXContextID;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
typedef XID GLXPbuffer;
typedef XID GLXWindow;
typedef XID GLXFBConfigID;

typedef struct __GLXcontextRec* GLXContext;
typedef struct __GLXFBConfigRec* GLXFBConfig;

// Visual Info Structure (Compatible with X11)
typedef struct {
    void* visual;
    VisualID visualid;
    int screen;
    int depth;
    int class_type;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

// GLX Constants
#define GLX_USE_GL              1
#define GLX_BUFFER_SIZE         2
#define GLX_LEVEL               3
#define GLX_RGBA                4
#define GLX_DOUBLEBUFFER        5
#define GLX_STEREO              6
#define GLX_AUX_BUFFERS         7
#define GLX_RED_SIZE            8
#define GLX_GREEN_SIZE          9
#define GLX_BLUE_SIZE           10
#define GLX_ALPHA_SIZE          11
#define GLX_DEPTH_SIZE          12
#define GLX_STENCIL_SIZE        13
#define GLX_ACCUM_RED_SIZE      14
#define GLX_ACCUM_GREEN_SIZE    15
#define GLX_ACCUM_BLUE_SIZE     16
#define GLX_ACCUM_ALPHA_SIZE    17

// --------------------------------------------------------------------------
// GLX Bridge Class
// --------------------------------------------------------------------------
class GLXBridge {
public:
    // --- GLX Core Functions ---
    static XVisualInfo* ChooseVisual(Display* dpy, int screen, int* attribList);
    static GLXContext CreateContext(Display* dpy, XVisualInfo* vis, GLXContext shareList, bool direct);
    static void DestroyContext(Display* dpy, GLXContext ctx);
    static int MakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx);
    static void SwapBuffers(Display* dpy, GLXDrawable drawable);
    static int SwapInterval(int interval);
    static void* GetProcAddress(const char* procName);

    // --- GLX Query / Info ---
    static const char* QueryExtensionsString(Display* dpy, int screen);
    static const char* QueryServerString(Display* dpy, int screen, int name);
    static const char* GetClientString(Display* dpy, int name);
    static Display* GetCurrentDisplay();
    static GLXContext GetCurrentContext();
    static GLXDrawable GetCurrentDrawable();
    static int IsDirect(Display* dpy, GLXContext ctx);
    static int GetConfig(Display* dpy, XVisualInfo* vis, int attrib, int* value);

    // --- GLX SGIX / FBConfig Extensions ---
    static GLXFBConfig* ChooseFBConfigSGIX(Display* dpy, int screen, int* attrib_list, int* nelements);
    static int GetFBConfigAttribSGIX(Display* dpy, GLXFBConfig config, int attribute, int* value);
    static XVisualInfo* GetVisualFromFBConfig(Display* dpy, GLXFBConfig config);
    static GLXContext CreateContextWithConfigSGIX(Display* dpy, GLXFBConfig config, int render_type, GLXContext share_list, bool direct);

    // PBuffer Stubs
    static GLXPbuffer CreateGLXPbufferSGIX(Display* dpy, GLXFBConfig config, unsigned int width, unsigned int height, int* attrib_list);
    static void DestroyGLXPbufferSGIX(Display* dpy, GLXPbuffer pbuf);

    // --- GLUT (Toolkit) Integration ---
    static void glutInit(int* argcp, char** argv);
    static void glutInitDisplayMode(unsigned int mode);
    static void glutInitWindowSize(int width, int height);
    static void glutInitWindowPosition(int x, int y);
    static int glutCreateWindow(const char* title);
    static int glutEnterGameMode();
    static void glutLeaveGameMode();
    static void glutMainLoop();
    static void glutDisplayFunc(void (*func)(void));
    static void glutIdleFunc(void (*func)(void));
    static void glutPostRedisplay();
    static void glutSwapBuffers();
    static int glutGet(int state);
    static void glutSetCursor(int cursor);
    static void glutGameModeString(const char* string);
    static void glutBitmapCharacter(void* font, int character);
    static int glutBitmapWidth(void* font, int character);

    // --- GLU Utilities ---
    static void gluPerspective(double fovy, double aspect, double zNear, double zFar);
    static void gluLookAt(double eyeX, double eyeY, double eyeZ, double centerX, double centerY, double centerZ, double upX, double upY, double upZ);
};