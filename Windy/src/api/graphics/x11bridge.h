#pragma once
#include "x11types.h"

struct SDL_Window;

class X11Bridge
{
  public:
    // --- Display & Window Management ---
    static Display *XOpenDisplay(const char *name);
    static int XCloseDisplay(Display *dpy);
    static Window XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height,
                                unsigned int border_width, int depth, unsigned int class_type, void *visual, unsigned long valuemask,
                                XSetWindowAttributes *attributes);
    static int XDestroyWindow(Display *dpy, Window win);
    static void XMapWindow(Display *dpy, Window win);
    static int XMoveWindow(Display *dpy, Window win, int x, int y);
    static int XResizeWindow(Display *dpy, Window win, unsigned int width, unsigned int height);
    static int XSync(Display *dpy, int discard);
    static int XFlush(Display *dpy);
    static void XLockDisplay(Display *dpy);
    static void XUnlockDisplay(Display *dpy);
    static void XSetErrorHandler(void *handler);
    static int XGetErrorText(Display *dpy, int code, char *buffer, int length);

    static int XDisplayWidth(void *dpy, int screen_number);
    static int XDisplayHeight(void *dpy, int screen_number);
    static int XDisplayWidthMM(void *dpy, int screen_number);
    static int XDisplayHeightMM(void *dpy, int screen_number);
    static Atom XInternAtom(Display *dpy, const char *atom_name, bool create_if_not_exist);
    static int XSetStandardProperties(Display *dpy, Window win, const char *window_name, const char *icon_name, Pixmap icon_pixmap,
                                      char **argv, int argc, XSetWindowAttributes *attributes);

    static Atom XCreateColormap(Display *dpy, Window w, Visual *visual, int alloc);
    static void *XextFindDisplay(Display *dpy);

    // --- Event Handling ---
    static int XPending(Display *dpy);
    static void XNextEvent(Display *dpy, XEvent *event_return);
    static void XSendEvent(Display *dpy, Window w, bool propagate, long event_mask, XEvent *event_send);
    static int XCheckTypedEvent(Display *dpy, int type, XEvent *event_return);
    static int XSelectInput(Display *dpy, Window win, long event_mask);
    static int XSetCloseDownMode(Display *dpy, int close_mode);
    static int XSetInputFocus(Display *dpy, Window win, int revert_to, Time time);
    static int XAutoRepeatOff(Display *dpy);

    // --- Input (Mouse/Keyboard) ---
    static int XQueryPointer(Display *dpy, Window win, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return,
                             int *win_x_return, int *win_y_return, unsigned int *mask_return);
    static void XWarpPointer(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width,
                             unsigned int src_height, int dest_x, int dest_y);
    static void XGrabPointer(Display *dpy, Window win, int owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode,
                             Window confine_to, Cursor cursor, Time time);
    static void XUngrabPointer(Display *dpy, Time time);
    static int XGrabKeyboard(Display *dpy, Window win, int owner_events, int pointer_mode, int keyboard_mode, Time time);
    static int XUngrabKeyboard(Display *dpy, Time time);
    static int XAutoRepeatOn(Display *dpy);
    static int XKeysymToKeycode(Display *dpy, int keysym);

    // --- Attributes, Properties & WM Hints ---
    static int XGetWindowAttributes(Display *dpy, Window win, XWindowAttributes *window_attributes_return);
    static int XGetGeometry(Display *dpy, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return,
                            unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return);
    static Atom XInternAtom(Display *dpy, const char *name, int only_if_exists);
    static int XStoreName(Display *dpy, Window win, const char *name);
    static int XChangeProperty(Display *dpy, Window win, Atom property, Atom type, int format, int mode, const unsigned char *data,
                               int nelements);
    static int XParseColor(Display *dpy, Colormap colormap, const char *spec, int *color_return);

    static void XMapRaised(Display *dpy, Window w);
    static void *XRRGetScreenResources(Display *dpy, Window window);
    static void *XRRGetCrtcInfo(Display *dpy, int crtc);
    static int XLookupString(XKeyEvent *key_event, char *buffer_return, int buffer_size, int *keysym_return, int *status_return);
    static int XTranslateCoordinates(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, int *dest_x_return,
                                     int *dest_y_return, Window *child_return);
    // Missing members added:
    static int XSetWMProtocols(Display *dpy, Window win, Atom *protocols, int count);
    static int XSetWMProperties(Display *dpy, Window win, XTextProperty *window_name, XTextProperty *icon_name, char **argv, int argc,
                                XSizeHints *normal_hints, XWMHints *wm_hints, XClassHint *class_hints);
    static void XSetWMNormalHints(Display *dpy, Window w, XSizeHints *hints);
    static void XSetTransientForHint(Display *dpy, Window w, Window transient_for);
    static int XStringListToTextProperty(char **list, int count, XTextProperty *text_prop_return);

    // --- Pixmaps & Cursors ---
    static Pixmap XCreatePixmapFromBitmapData(Display *dpy, Drawable drawable, char *data, unsigned int width, unsigned int height,
                                              unsigned long fg, unsigned long bg, unsigned int depth);
    static Cursor XCreatePixmapCursor(Display *dpy, Pixmap source, Pixmap mask, void *foreground_color, void *background_color,
                                      unsigned int x, unsigned int y);
    static int XFreePixmap(Display *dpy, Pixmap pixmap);
    static int XFreeCursor(Display *dpy, Cursor cursor);
    static void XDefineCursor(Display *dpy, Window w, Cursor c);
    static Cursor XCreateFontCursor(Display *dpy, int shape);
    static Pixmap XCreateBitmapFromData(Display *dpy, Drawable drawable, char *data, unsigned int width, unsigned int height);

    // --- XF86VidMode Extension (Stubs) ---
    static int XF86VidModeQueryExtension(Display *dpy, int *event_base_return, int *error_base_return);
    static int XF86VidModeGetAllModeLines(Display *dpy, int screen, int *modecount_return, XF86VidModeModeInfo ***modesinfo_return);
    static int XF86VidModeGetModeLine(Display *dpy, int screen, int *dotclock_return, void *modeline);
    static int XF86VidModeSwitchToMode(Display *dpy, int screen, void *modeline);
    static int XF86VidModeSetViewPort(Display *dpy, int screen, int x, int y);
    static int XF86VidModeGetViewPort(Display *dpy, int screen, int *x_return, int *y_return);
    static int XF86VidModeLockModeSwitch(Display *dpy, int screen, int lock);
    static int XF86VidModeQueryVersion(Display *dpy, int *major_return, int *minor_return);

    // --- Misc / Legacy Stubs ---
    static int XInitThreads();
    static void XFree(void *data);
    static int _XReply(Display *dpy, int *rep, int size, int expect_reply);

    // --- Internal Integration ---
    static SDL_Window *GetSDLWindow(Window win);
    static Window GetXWindowFromSDL(SDL_Window *sdlWin);
};
