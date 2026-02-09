#pragma once
#include "x11types.h"

struct SDL_Window;

class X11Bridge
{
  public:
    // --- Display & Window Management ---
    static Display *OpenDisplay(const char *name);
    static int CloseDisplay(Display *dpy);
    static Window XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height,
                                unsigned int border_width, int depth, unsigned int class_type, void *visual, unsigned long valuemask,
                                XSetWindowAttributes *attributes);
    static int DestroyWindow(Display *dpy, Window win);
    static void MapWindow(Display *dpy, Window win);
    static int MoveWindow(Display *dpy, Window win, int x, int y);
    static int ResizeWindow(Display *dpy, Window win, unsigned int width, unsigned int height);
    static int Sync(Display *dpy, int discard);
    static int Flush(Display *dpy);
    static void LockDisplay(Display *dpy);
    static void UnlockDisplay(Display *dpy);
    static void SetErrorHandler(void *handler);
    static int GetErrorText(Display *dpy, int code, char *buffer, int length);

    // --- Event Handling ---
    static int Pending(Display *dpy);
    static void NextEvent(Display *dpy, XEvent *event_return);
    static int CheckTypedEvent(Display *dpy, int type, XEvent *event_return);
    static int SelectInput(Display *dpy, Window win, long event_mask);
    static int SetCloseDownMode(Display *dpy, int close_mode);
    static int SetInputFocus(Display *dpy, Window win, int revert_to, Time time);
    static int TranslateCoordinates(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, int *dest_x_return, int *dest_y_return,
                                    Window *child_return);

    // --- Input (Mouse/Keyboard) ---
    static int QueryPointer(Display *dpy, Window win, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return,
                            int *win_x_return, int *win_y_return, unsigned int *mask_return);
    static void WarpPointer(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width,
                            unsigned int src_height, int dest_x, int dest_y);
    static void GrabPointer(Display *dpy, Window win, int owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode,
                            Window confine_to, Cursor cursor, Time time);
    static void UngrabPointer(Display *dpy, Time time);
    static int LookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return,
                            XComposeStatus *status_in_out);
    static int GrabKeyboard(Display *dpy, Window win, int owner_events, int pointer_mode, int keyboard_mode, Time time);
    static int UngrabKeyboard(Display *dpy, Time time);
    static int AutoRepeatOn(Display *dpy);

    // --- Attributes, Properties & WM Hints ---
    static int GetWindowAttributes(Display *dpy, Window win, XWindowAttributes *window_attributes_return);
    static int GetGeometry(Display *dpy, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return,
                           unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return);
    static Atom InternAtom(Display *dpy, const char *name, int only_if_exists);
    static int StoreName(Display *dpy, Window win, const char *name);
    static Colormap CreateColormap(Display *dpy, Window w, void *visual, int alloc);
    static int ParseColor(Display *dpy, Colormap colormap, const char *spec, XColor *color_return);
    static int ChangeProperty(Display *dpy, Window win, Atom property, Atom type, int format, int mode, const unsigned char *data,
                              int nelements);

    // Missing members added:
    static int SetWMProtocols(Display *dpy, Window win, Atom *protocols, int count);
    static int SetWMProperties(Display *dpy, Window win, XTextProperty *window_name, XTextProperty *icon_name, char **argv, int argc,
                               XSizeHints *normal_hints, XWMHints *wm_hints, XClassHint *class_hints);
    static int StringListToTextProperty(char **list, int count, XTextProperty *text_prop_return);

    // --- Pixmaps & Cursors ---
    static Pixmap CreatePixmapFromBitmapData(Display *dpy, Drawable drawable, char *data, unsigned int width, unsigned int height,
                                             unsigned long fg, unsigned long bg, unsigned int depth);
    static Cursor CreatePixmapCursor(Display *dpy, Pixmap source, Pixmap mask, void *foreground_color, void *background_color,
                                     unsigned int x, unsigned int y);
    static int FreePixmap(Display *dpy, Pixmap pixmap);
    static int FreeCursor(Display *dpy, Cursor cursor);
    static void DefineCursor(Display *dpy, Window w, Cursor c);

    // --- XF86VidMode Extension (Stubs) ---
    static int VidModeQueryExtension(Display *dpy, int *event_base_return, int *error_base_return);
    static int VidModeGetAllModeLines(Display *dpy, int screen, int *modecount_return, void ***modesinfo_return);
    static int VidModeGetModeLine(Display *dpy, int screen, int *dotclock_return, void *modeline);
    static int VidModeSwitchToMode(Display *dpy, int screen, void *modeline);
    static int VidModeSetViewPort(Display *dpy, int screen, int x, int y);
    static int VidModeGetViewPort(Display *dpy, int screen, int *x_return, int *y_return);
    static int VidModeLockModeSwitch(Display *dpy, int screen, int lock);

    // --- Misc / Legacy Stubs ---
    static int InitThreads();
    static void Free(void *data);

    // --- Internal Integration ---
    static SDL_Window *GetSDLWindow(Window win);
    static Window GetXWindowFromSDL(SDL_Window *sdlWin);
};