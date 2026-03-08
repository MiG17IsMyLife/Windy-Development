#define _CRT_SECURE_NO_WARNINGS

#include "sdlcalls.h"
#include "x11bridge.h"
#include "../../core/config.h"
#include "../../core/log.h"
#include <SDL3/SDL.h>
#include <deque>
#include <map>
#include <mutex>
#include <cstring>
#include <cstdlib>

// Windows API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// --------------------------------------------------------------------------
// Internal X Server State Manager
// --------------------------------------------------------------------------
class XServerState
{
  public:
    static XServerState &Instance()
    {
        static XServerState instance;
        return instance;
    }

    void Initialize()
    {
        if (!sdlInitialized)
        {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
            {
                log_error("X11Bridge: SDL_Init failed: %s", SDL_GetError());
            }
            else
            {
                sdlInitialized = true;
                log_info("X11Bridge: SDL Subsystem Initialized");
            }
        }
    }

    Window CreateVirtualWindow(int width, int height)
    {
        if (SDLCalls::GetWindow())
        {
            SDL_SetWindowSize(SDLCalls::GetWindow(), width, height);
            RegisterWindow(SDLCalls::GetWindow());
            return mainWindowID;
        }

        SDLCalls::Start(0, 0);
        RegisterWindow(SDLCalls::GetWindow());
        return mainWindowID;
    }

    void RegisterWindow(SDL_Window *sdlWin)
    {
        if (reverseWindowMap.find(sdlWin) == reverseWindowMap.end())
        {
            mainWindowID = 1;
            windowMap[mainWindowID] = sdlWin;
            reverseWindowMap[sdlWin] = mainWindowID;
        }
    }

    SDL_Window *GetSDLWindow(Window win)
    {
        if (windowMap.find(win) != windowMap.end())
        {
            return windowMap[win];
        }
        return SDLCalls::GetWindow();
    }

    Window GetXID(SDL_Window *win)
    {
        if (reverseWindowMap.find(win) != reverseWindowMap.end())
        {
            return reverseWindowMap[win];
        }
        return 1;
    }

//     void PushEvent(const XEvent &ev)
//     {
//         std::lock_guard<std::mutex> lock(eventQueueMutex);
//         eventQueue.push_back(ev);
//     }

//     bool PopEvent(XEvent *outEv)
//     {
//         std::lock_guard<std::mutex> lock(eventQueueMutex);
//         if (eventQueue.empty())
//             return false;
//         *outEv = eventQueue.front();
//         eventQueue.pop_front();
//         return true;
//     }

//     bool HasEvents()
//     {
//         std::lock_guard<std::mutex> lock(eventQueueMutex);
//         return !eventQueue.empty();
//     }

//     void PumpEvents()
//     {
//         SDL_Event e;
//         while (SDL_PollEvent(&e))
//         {
//             TranslateAndPushEvent(e);
//         }
//     }

//   private:
//     void TranslateAndPushEvent(const SDL_Event &e)
//     {
//         XEvent xev = {0};
//         Window winID = GetXID(SDL_GetWindowFromID(e.window.windowID));

//         switch (e.type)
//         {
//             case SDL_EVENT_QUIT:
//                 log_info("X11Bridge: SDL_EVENT_QUIT");
//                 exit(0);
//                 break;
//             case SDL_EVENT_KEY_DOWN:
//             case SDL_EVENT_KEY_UP:
//                 xev.xkey.type = (e.type == SDL_EVENT_KEY_DOWN) ? KeyPress : KeyRelease;
//                 xev.xkey.window = winID;
//                 xev.xkey.keycode = e.key.scancode;
//                 xev.xkey.time = (Time)e.key.timestamp;
//                 xev.xkey.display = (Display *)0x1111;
//                 PushEvent(xev);
//                 break;
//             case SDL_EVENT_MOUSE_MOTION:
//                 xev.xmotion.type = MotionNotify;
//                 xev.xmotion.window = winID;
//                 xev.xmotion.x = (int)e.motion.x;
//                 xev.xmotion.y = (int)e.motion.y;
//                 xev.xmotion.time = (Time)e.motion.timestamp;
//                 xev.xmotion.display = (Display *)0x1111;
//                 PushEvent(xev);
//                 break;
//             case SDL_EVENT_MOUSE_BUTTON_DOWN:
//             case SDL_EVENT_MOUSE_BUTTON_UP:
//                 xev.xbutton.type = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? ButtonPress : ButtonRelease;
//                 xev.xbutton.window = winID;
//                 xev.xbutton.button = e.button.button;
//                 xev.xbutton.x = (int)e.button.x;
//                 xev.xbutton.y = (int)e.button.y;
//                 xev.xbutton.time = (Time)e.button.timestamp;
//                 xev.xbutton.display = (Display *)0x1111;
//                 PushEvent(xev);
//                 break;
//         }
//     }

    bool sdlInitialized = false;
    Window mainWindowID = 1;
    std::map<Window, SDL_Window *> windowMap;
    std::map<SDL_Window *, Window> reverseWindowMap;
    std::deque<XEvent> eventQueue;
    std::mutex eventQueueMutex;
};

// --------------------------------------------------------------------------
// X11Bridge Implementation
// --------------------------------------------------------------------------

int X11Bridge::XInitThreads()
{
    return 1;
}


static char s_dummyDisplay[4096] = {0};
static char s_commandBuffer[8192] = {0};
static char s_dummyScreens[4096] = {0};

Display *X11Bridge::XOpenDisplay(const char *name)
{
    printf("XOpenDisplay(\"%s\")\n", name ? name : "NULL");
    XServerState::Instance().Initialize();
    *(int*)(s_dummyDisplay + 0x84) = 0; // Offset 0x84: Default Screen (int)
    *(void**)(s_dummyDisplay + 0x8C) = s_dummyScreens;  // Offset 0x8C: Screens pointer
    *(Window*)(s_dummyScreens + 0x08) = 0; // Offset 0x08 : window ID 
    *(int*)(s_dummyScreens + 0x0C) = getConfig()->width;  // Offset 0x0C: Width  
    *(int*)(s_dummyScreens + 0x10) = getConfig()->height; // Offset 0x10: Height
    *(char**)(s_dummyDisplay + 0x6C) = s_commandBuffer; // Offset 0x6C: Current buffer pointer??
    *(char**)(s_dummyDisplay + 0x70) = s_commandBuffer + sizeof(s_commandBuffer); // Offset 0x70: bufmax??
    return (Display *)&s_dummyDisplay;
}


// Display *X11Bridge::XOpenDisplay(const char *name)
// {
//     log_info("XOpenDisplay(\"%s\")", name ? name : "NULL");

//     // Initialize SDL and Create Hidden Window + Context
//     SDLCalls::Init();
//     SDLCalls::CreateSDLWindow(true);

//     // Allocate Display structure
//     Display *dpy = new Display();
//     memset(dpy, 0, sizeof(Display));

//     // Allocate Screen structure (usually one screen)
//     dpy->nscreens = 1;
//     dpy->default_screen = 0;
//     dpy->screens = new Screen[1];
//     memset(dpy->screens, 0, sizeof(Screen));

//     // Setup Screen 0 with valid Config dimensions
//     Screen *scr = &dpy->screens[0];
//     scr->display = dpy;
//     scr->root = 0; // Root window ID (can be 0 or 1)
//     scr->width = getConfig()->width;
//     scr->height = getConfig()->height;
//     scr->mwidth = (scr->width * 25.4) / 96.0;
//     scr->mheight = (scr->height * 25.4) / 96.0;
//     scr->root_depth = 24;
//     scr->white_pixel = 0xFFFFFFFF;
//     scr->black_pixel = 0x00000000;

//     // Fill common Display fields
//     dpy->fd = -1; // No real socket
//     dpy->proto_major_version = 11;
//     dpy->proto_minor_version = 0;
//     dpy->vendor = (char *)"Windy X11 Bridge";
//     dpy->byte_order = 1; // LSBFirst (Windows is Little Endian)
//     dpy->bitmap_unit = 32;
//     dpy->bitmap_pad = 32;
//     dpy->bitmap_bit_order = 1; // LSBFirst
//     dpy->nformats = 0;

//     // Return valid pointer
//     return dpy;
// }

int X11Bridge::XCloseDisplay(Display *dpy)
{
    if (dpy)
    {
        if (dpy->screens)
        {
            delete[] dpy->screens;
        }
        delete dpy;
    }
    return 0;
}

Window X11Bridge::XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height,
                                unsigned int border_width, int depth, unsigned int class_type, void *visual, unsigned long valuemask,
                                XSetWindowAttributes *attributes)
{
    printf("XCreateWindow: %ux%u\n", width, height);

    // Window already created in XOpenDisplay (hidden).
    // We can update its size or position here if suitable, or just return the existing handle.
    // For now, assume the Config size is what we want and just show it later in MapWindow.
    if (SDLCalls::GetWindow())
    {
        SDL_SetWindowSize(SDLCalls::GetWindow(), width, height);
    }

    return XServerState::Instance().CreateVirtualWindow(width, height);
}

int X11Bridge::XDestroyWindow(Display *dpy, Window win)
{
    return 1;
}

void X11Bridge::XMapWindow(Display *dpy, Window win)
{
    printf("XMapWindow Called\n");
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_ShowWindow(sdlWin);
    return; 
}

int X11Bridge::_XReply(Display *dpy, int *rep, int extra, int discard)
{
    if (rep) {
        memset(rep, 0, 32);
        ((char*)rep)[0] = 1; // type = X_Reply
        
        unsigned short* p = (unsigned short*)((char*)rep + 4); //offset 4 and 6 for version??
        p[0] = 2; // Major
        p[1] = 2; // Minor
    }
    return 1;
}

int X11Bridge::XMoveWindow(Display *dpy, Window win, int x, int y)
{
    if (SDLCalls::GetWindow())
    {
        SDL_SetWindowPosition(SDLCalls::GetWindow(), x, y);
    }
    return 1;
}

int X11Bridge::XResizeWindow(Display *dpy, Window win, unsigned int width, unsigned int height)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_SetWindowSize(sdlWin, width, height);
    return 1;
}

int X11Bridge::XSync(Display *dpy, int discard)
{
    return 1;
}
int X11Bridge::XFlush(Display *dpy)
{
    return 1;
}

void X11Bridge::XLockDisplay(Display *dpy)
{
}
void X11Bridge::XUnlockDisplay(Display *dpy)
{
}
void X11Bridge::XSetErrorHandler(void *handler)
{
}
int X11Bridge::XGetErrorText(Display *dpy, int code, char *buffer, int length)
{
    if (buffer && length > 0)
        buffer[0] = '\0';
    return 0;
}

int X11Bridge::XDisplayWidth(void *dpy, int screen_number)
{
    // Return the width from our config (or current window size)
    return getConfig()->width;
}

int X11Bridge::XDisplayHeight(void *dpy, int screen_number)
{
    // Return the height from our config
    return getConfig()->height;
}

int X11Bridge::XDisplayWidthMM(void *dpy, int screen_number)
{
    // Approximation: 96 DPI
    // Width (mm) = (Pixels * 25.4) / DPI
    return (int)((getConfig()->width * 25.4f) / 96.0f);
}

int X11Bridge::XDisplayHeightMM(void *dpy, int screen_number)
{
    // Approximation: 96 DPI
    return (int)((getConfig()->height * 25.4f) / 96.0f);
}

Atom X11Bridge::XInternAtom(Display *dpy, const char *atom_name, bool create_if_not_exist)
{
    return 1;
}

static int s_dummyExtCodes[2] = { 0, 128 };
static void* s_dummyExtData[4] = { (void*)1, (void*)1, (void*)s_dummyExtCodes, (void*)1 };
void *X11Bridge::XextFindDisplay(Display *dpy)
{
    printf("XextFindDisplay\n");
    return s_dummyExtData;  // offset 0x8 must be non-zero
}

// --- Event Handling ---

int X11Bridge::XPending(Display *dpy)
{
    // XServerState::Instance().PumpEvents();
    // return XServerState::Instance().HasEvents() ? 1 : 0;
    return 0;
}

void X11Bridge::XNextEvent(Display *dpy, XEvent *event_return)
{
    // while (true)
    // {
    //     XServerState::Instance().PumpEvents();
    //     if (XServerState::Instance().PopEvent(event_return))
    //         return;
    //     SDL_Delay(1);
    // }
    return;
}

void X11Bridge::XSendEvent(Display *dpy, Window w, bool propagate, long event_mask, XEvent *event_send)
{
    return;
}

int X11Bridge::XCheckTypedEvent(Display *dpy, int type, XEvent *event_return)
{
    // XServerState::Instance().PumpEvents();
    return 0; // Simplified
}

int X11Bridge::XSelectInput(Display *dpy, Window win, long event_mask)
{
    return 0;
}
int X11Bridge::XSetCloseDownMode(Display *dpy, int close_mode)
{
    return 1;
}

int X11Bridge::XSetInputFocus(Display *dpy, Window win, int revert_to, Time time)
{
    // SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    // if (sdlWin)
    //     SDL_RaiseWindow(sdlWin);
    return 1;
}

int X11Bridge::XAutoRepeatOff(Display *dpy)
{
    return 1;
}

int X11Bridge::XKeysymToKeycode(Display *dpy, int keysym)
{
    return 0;
}

int X11Bridge::XTranslateCoordinates(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, int *dest_x_return, int *dest_y_return,
                                     Window *child_return)
{
    SDL_Window *srcWin = XServerState::Instance().GetSDLWindow(src_w);
    SDL_Window *destWin = XServerState::Instance().GetSDLWindow(dest_w);

    int srcWinX = 0;
    int srcWinY = 0;
    int destWinX = 0;
    int destWinY = 0;

    if (srcWin)
        SDL_GetWindowPosition(srcWin, &srcWinX, &srcWinY);
    if (destWin)
        SDL_GetWindowPosition(destWin, &destWinX, &destWinY);

    int globalX = src_x + srcWinX;
    int globalY = src_y + srcWinY;

    if (dest_x_return)
        *dest_x_return = globalX - destWinX;
    if (dest_y_return)
        *dest_y_return = globalY - destWinY;
    if (child_return)
        *child_return = 0;
    return 1;
}

// --- Input (Mouse/Keyboard) ---

int X11Bridge::XQueryPointer(Display *dpy, Window win, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return,
                             int *win_x_return, int *win_y_return, unsigned int *mask_return)
{
    // float mx, my;
    // uint32_t mask = SDL_GetGlobalMouseState(&mx, &my);

    // if (root_x_return)
    //     *root_x_return = (int)mx;
    // if (root_y_return)
    //     *root_y_return = (int)my;

    // SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    // if (sdlWin)
    // {
    //     int wx, wy;
    //     SDL_GetWindowPosition(sdlWin, &wx, &wy);
    //     if (win_x_return)
    //         *win_x_return = (int)mx - wx;
    //     if (win_y_return)
    //         *win_y_return = (int)my - wy;
    // }

    // if (mask_return)
    //     *mask_return = mask;
    return 0;
}

void X11Bridge::XWarpPointer(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width,
                             unsigned int src_height, int dest_x, int dest_y)
{
//     SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(dest_w);
//     if (sdlWin)
//         SDL_WarpMouseInWindow(sdlWin, (float)dest_x, (float)dest_y);
}

void X11Bridge::XGrabPointer(Display *dpy, Window win, int owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode,
                             Window confine_to, Cursor cursor, Time time)
{
    // SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    // if (sdlWin)
    //     SDL_SetWindowMouseGrab(sdlWin, true);
}

void X11Bridge::XUngrabPointer(Display *dpy, Time time)
{
    // if (SDLCalls::GetWindow())
    //     SDL_SetWindowMouseGrab(SDLCalls::GetWindow(), false);
}

int X11Bridge::XGrabKeyboard(Display *dpy, Window win, int owner_events, int pointer_mode, int keyboard_mode, Time time)
{
    // SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    // if (sdlWin)
    // {
    //     SDL_SetWindowKeyboardGrab(sdlWin, true);
    //     return 0; // Success
    // }
    return 0;
}

int X11Bridge::XUngrabKeyboard(Display *dpy, Time time)
{
    // if (SDLCalls::GetWindow())
    //     SDL_SetWindowKeyboardGrab(SDLCalls::GetWindow(), false);
    return 0;
}

int X11Bridge::XAutoRepeatOn(Display *dpy)
{
    return 1;
}

// --- Attributes & Properties ---

int X11Bridge::XGetWindowAttributes(Display *dpy, Window win, XWindowAttributes *attr)
{
    printf("XGetWindowAttributes Called\n");
    if (!attr)
        return 0;
    memset(attr, 0, sizeof(XWindowAttributes));
    attr->x = 0;
    attr->y = 0;
    attr->width = getConfig()->width;
    attr->height = getConfig()->height;
    attr->depth = 24;
    attr->visual = (void *)1;
    attr->map_state = 2; // IsViewable
    return 1;
}

int X11Bridge::XGetGeometry(Display *dpy, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return,
                            unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return)
{
    printf("XGetGeometry Called\n");
    if (root_return)
        *root_return = 0;
    if (x_return)
        *x_return = 0;
    if (y_return)
        *y_return = 0;
    if (width_return)
        *width_return = getConfig()->width;
    if (height_return)
        *height_return = getConfig()->height;
    if (depth_return)
        *depth_return = 24;
    if (border_width_return)
        *border_width_return = 0;
    return 1;
}

int X11Bridge::XSetStandardProperties(Display *dpy, Window win, const char *window_name, const char *icon_name, Pixmap icon_pixmap,
                                      char **argv, int argc, XSetWindowAttributes *attributes)
{
    printf("XSetStandardProperties Called\n");
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin && window_name)
    {
        SDL_SetWindowTitle(sdlWin, "SEGA Lindbergh Emulator");
    }
    return 1;
}

int X11Bridge::XChangeProperty(Display *dpy, Window win, Atom property, Atom type, int format, int mode, const unsigned char *data,
                               int nelements)
{
    return 1;
}

int X11Bridge::XParseColor(Display *dpy, Colormap colormap, const char *spec, int *color_return)
{
    return 1;
} 

void X11Bridge::XMapRaised(Display *dpy, Window w)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(w);
    if (sdlWin)
        SDL_RaiseWindow(sdlWin);
}

static char s_dummyScreenResources[1024] = {0};
static char s_dummyCrtcs[1024] = {0};

void *X11Bridge::XRRGetScreenResources(Display *dpy, Window window)
{
    printf("XRRGetScreenResources Called\n");
    // Offset 0x0C: pointer to an array of CRTCs. Dereferencing this pointer
    // twice yields the first CRTC ID.
    *(void **)(s_dummyScreenResources + 0x0C) = s_dummyCrtcs;
    return s_dummyScreenResources;
}

static char s_dummyCrtcInfo[1024] = {0};

void *X11Bridge::XRRGetCrtcInfo(Display *dpy, int crtc)
{
    printf("XRRGetCrtcInfo Called\n");
    // Offset 0x0C: Width
    *(unsigned int *)(s_dummyCrtcInfo + 0x0C) = getConfig()->width;
    // Offset 0x10: Height
    *(unsigned int *)(s_dummyCrtcInfo + 0x10) = getConfig()->height;
    return s_dummyCrtcInfo;
}

int X11Bridge::XLookupString(XKeyEvent *key_event, char *buffer_return, int buffer_size, int *keysym_return, int *status_return)
{
    return 0;
}

int X11Bridge::XStoreName(Display *dpy, Window win, const char *name)
{
    printf("XStoreName Called\n");
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin && name)
        SDL_SetWindowTitle(sdlWin, name);
    return 0;
}

int X11Bridge::XSetWMProtocols(Display *dpy, Window win, Atom *protocols, int count)
{
    return 1;
}

int X11Bridge::XSetWMProperties(Display *dpy, Window win, XTextProperty *window_name, XTextProperty *icon_name, char **argv, int argc,
                                XSizeHints *normal_hints, XWMHints *wm_hints, XClassHint *class_hints)
{
    return 1;
}

void X11Bridge::XSetWMNormalHints(Display *dpy, Window w, XSizeHints *hints)
{
    printf("XSetWMNormalHints Called\n");
}

void X11Bridge::XSetTransientForHint(Display *dpy, Window w, Window transient_for)
{
    printf("XSetTransientForHint Called\n");
}

int X11Bridge::XStringListToTextProperty(char **list, int count, XTextProperty *text_prop_return)
{
    if (text_prop_return && count > 0 && list && list[0])
    {
        size_t len = strlen(list[0]);
        // Note: In a real X11 impl, XFree should free this. We use _aligned_malloc in original, here just malloc.
        text_prop_return->value = (unsigned char *)malloc(len + 1);
        if (text_prop_return->value)
            strcpy((char *)text_prop_return->value, list[0]);
        text_prop_return->encoding = 31;
        text_prop_return->format = 8;
        text_prop_return->nitems = len;
        return 1;
    }
    return 0;
}

void X11Bridge::XFree(void *data)
{
    if (data)
        free(data);
}

// --- Pixmaps & Cursors ---

Pixmap X11Bridge::XCreatePixmapFromBitmapData(Display *dpy, Drawable drawable, char *data, unsigned int width, unsigned int height,
                                              unsigned long fg, unsigned long bg, unsigned int depth)
{
    return 2001;
}

Cursor X11Bridge::XCreatePixmapCursor(Display *dpy, Pixmap source, Pixmap mask, void *foreground_color, void *background_color,
                                      unsigned int x, unsigned int y)
{
    return 1001;
}

int X11Bridge::XFreePixmap(Display *dpy, Pixmap pixmap)
{
    return 1;
}

int X11Bridge::XFreeCursor(Display *dpy, Cursor cursor)
{
    return 1;
}
void X11Bridge::XDefineCursor(Display *dpy, Window w, Cursor c)
{
}

Cursor X11Bridge::XCreateFontCursor(Display *dpy, int shape)
{
    return 1001;
}

Pixmap X11Bridge::XCreateBitmapFromData(Display *dpy, Drawable drawable, char *data, unsigned int width, unsigned int height)
{
    return 2002;
}


// --- XF86VidMode Extension (Stubs) ---

int X11Bridge::XF86VidModeQueryExtension(Display *dpy, int *event_base_return, int *error_base_return)
{
    if (event_base_return)
        *event_base_return = 0;
    if (error_base_return)
        *error_base_return = 0;
    return 1;
}

int X11Bridge::XF86VidModeGetAllModeLines(Display *dpy, int screen, int *modecount_return, XF86VidModeModeInfo ***modesinfo_return)
{
    if (modecount_return)
        *modecount_return = 1;

    if (modesinfo_return)
    {
        XF86VidModeModeInfo **modes = *modesinfo_return;//(XF86VidModeModeInfo **)malloc(sizeof(XF86VidModeModeInfo *) * 1);

        modes[0] = (XF86VidModeModeInfo *)malloc(sizeof(XF86VidModeModeInfo));
        memset(modes[0], 0, sizeof(XF86VidModeModeInfo));

        modes[0]->hdisplay = (unsigned short)getConfig()->width;
        modes[0]->vdisplay = (unsigned short)getConfig()->height;
        modes[0]->dotclock = 60000; // Fake 60Hz dotclock
        modes[0]->htotal = modes[0]->hdisplay + 20;
        modes[0]->vtotal = modes[0]->vdisplay + 10;

        // *modesinfo_return = (void **)modes;
    }
    return 1;
}

int X11Bridge::XF86VidModeGetModeLine(Display *dpy, int screen, int *dotclock_return, void *modeline)
{
    if (dotclock_return)
        *dotclock_return = 0;
    return 1;
}

int X11Bridge::XF86VidModeSwitchToMode(Display *dpy, int screen, void *modeline)
{
    log_info("XF86VidModeSwitchToMode (Stub)");
    return 1;
}

int X11Bridge::XF86VidModeSetViewPort(Display *dpy, int screen, int x, int y)
{
    return 1;
}

int X11Bridge::XF86VidModeGetViewPort(Display *dpy, int screen, int *x_return, int *y_return)
{
    if (x_return)
        *x_return = 0;
    if (y_return)
        *y_return = 0;
    return 1;
}

int X11Bridge::XF86VidModeLockModeSwitch(Display *dpy, int screen, int lock)
{
    return 1;
}

int X11Bridge::XF86VidModeQueryVersion(Display *dpy, int *major_return, int *minor_return)
{
    if (major_return)
        *major_return = 0;
    if (minor_return)
        *minor_return = 0;
    return 1;
}

// Stub for XCreateColormap
Atom X11Bridge::XCreateColormap(Display *dpy, Window w, Visual *visual, int alloc)
{
    printf("XCreateColormap Called\n");
    return 1;
}

// Integration
SDL_Window *X11Bridge::GetSDLWindow(Window win)
{
    return XServerState::Instance().GetSDLWindow(win);
}

Window X11Bridge::GetXWindowFromSDL(SDL_Window *sdlWin)
{
    return XServerState::Instance().GetXID(sdlWin);
}
