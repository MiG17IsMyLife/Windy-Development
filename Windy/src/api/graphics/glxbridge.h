#ifndef __GLX_BRIDGE_H
#define __GLX_BRIDGE_H

class GLXBridge {
public:
    // Existing GLX
    static void* ChooseVisual(void* dpy, int screen, int* attribList);
    static void* CreateContext(void* dpy, void* vis, void* shareList, bool direct);
    static void DestroyContext(void* dpy, void* ctx);
    static int MakeCurrent(void* dpy, unsigned long drawable, void* ctx);
    static void SwapBuffers(void* dpy, unsigned long drawable);
    static int SwapInterval(int interval);
    static void* GetProcAddress(const char* procName);

    static const char* QueryExtensionsString(void* dpy, int screen);
    static const char* QueryServerString(void* dpy, int screen, int name);
    static const char* GetClientString(void* dpy, int name);

    static void* GetCurrentDisplay();
    static void* GetCurrentContext();
    static unsigned long GetCurrentDrawable();
    static int IsDirect(void* dpy, void* ctx);

    // SGIX Extensions
    static void* ChooseFBConfigSGIX(void* dpy, int screen, int* attrib_list, int* nelements);
    static int GetFBConfigAttribSGIX(void* dpy, void* config, int attribute, int* value);
    static void* CreateContextWithConfigSGIX(void* dpy, void* config, int render_type, void* share_list, bool direct);
    static void* CreateGLXPbufferSGIX(void* dpy, void* config, unsigned int width, unsigned int height, int* attrib_list);
    static void DestroyGLXPbufferSGIX(void* dpy, void* pbuf);

    static void glutInit(int* argcp, char** argv);
    static void glutInitDisplayMode(unsigned int mode);
    static void glutInitWindowSize(int width, int height);
    static void glutInitWindowPosition(int x, int y);
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

    // Bitmap / Font Stubs
    static void glutBitmapCharacter(void* font, int character);
    static int glutBitmapWidth(void* font, int character);

    static void gluPerspective(double fovy, double aspect, double zNear, double zFar);
    static void gluLookAt(double eyeX, double eyeY, double eyeZ, double centerX, double centerY, double centerZ, double upX, double upY, double upZ);
};

#endif