#define _CRT_SECURE_NO_WARNINGS

#include "SDL3/SDL_stdinc.h"
#include "sdlcalls.h"

#include "x11bridge.h"
#include "../../core/log.h"
#include <SDL3/SDL.h>
#include <deque>
#include <map>
#include <mutex>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>

// Windows API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma pack(push, 1)
struct FakeScreen
{
    uint8_t pad_00[0x08];        // 0x00 - 0x07
    uint32_t root;               // 0x08
    uint32_t width;              // 0x0C
    uint32_t height;             // 0x10
    uint32_t width_mm;           // 0x14
    uint32_t height_mm;          // 0x18
    uint8_t pad_1c[0x30 - 0x1c]; // 0x1C - 0x2F
    uint32_t colormap;           // 0x30
    uint8_t pad_34[0x50 - 0x34]; // 0x34 - 0x4F
}; // Total: 0x50 bytes

struct FakeDisplayData
{
    uint8_t pad_00[0x08];        // 0x00 - 0x07
    uint32_t connection;         // 0x08
    uint8_t pad_0c[0x84 - 0x0c]; // 0x0C - 0x83
    uint32_t screen;             // 0x84 (default screen number)
    uint8_t pad_88[0x8c - 0x88]; // 0x88 - 0x8B
    uint32_t screens_ptr;        // 0x8c (pointer to FakeScreen array)
};
#pragma pack(pop)

static FakeScreen g_fakeScreen;
static FakeDisplayData g_fakeDisplay;
static bool g_fakeDisplayInitialized = false;

static Display *InitFakeDisplay()
{
    if (g_fakeDisplayInitialized)
        return reinterpret_cast<Display *>(&g_fakeDisplay);

    memset(&g_fakeDisplay, 0, sizeof(g_fakeDisplay));
    memset(&g_fakeScreen, 0, sizeof(g_fakeScreen));

    // Get screen dimensions from SDL
    int screenW = 640, screenH = 480;
    SDL_Window *sdlWin = SDLCalls::GetWindow();
    if (sdlWin)
    {
        SDL_GetWindowSize(sdlWin, &screenW, &screenH);
    }
    else
    {
        int numDisplays = 0;
        SDL_DisplayID *displays = SDL_GetDisplays(&numDisplays);
        if (displays && numDisplays > 0)
        {
            const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(displays[0]);
            if (mode)
            {
                screenW = mode->w;
                screenH = mode->h;
            }
            SDL_free(displays);
        }
    }

    // Populate FakeScreen (offset 0x50 per entry)
    g_fakeScreen.root = 1; // fake root window ID
    g_fakeScreen.width = static_cast<uint32_t>(screenW);
    g_fakeScreen.height = static_cast<uint32_t>(screenH);
    g_fakeScreen.width_mm = screenW * 254 / 960; // ~96 DPI approximation
    g_fakeScreen.height_mm = screenH * 254 / 960;
    g_fakeScreen.colormap = 1; // fake default colormap

    // Populate FakeDisplay
    g_fakeDisplay.connection = 1; // fake connection fd
    g_fakeDisplay.screen = 0;     // default screen number = 0
    g_fakeDisplay.screens_ptr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_fakeScreen));

    g_fakeDisplayInitialized = true;

    log_info("FakeDisplay initialized: screen=%dx%d (%dmm x %dmm), root=%u", screenW, screenH, g_fakeScreen.width_mm,
             g_fakeScreen.height_mm, g_fakeScreen.root);

    return reinterpret_cast<Display *>(&g_fakeDisplay);
}

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

    Display *GetDisplay() const
    {
        return reinterpret_cast<Display *>(const_cast<FakeDisplayData *>(&g_fakeDisplay));
    }

    Window CreateVirtualWindow(int x, int y, int width, int height)
    {
        SDL_Window *sdlWin = SDLCalls::GetWindow();
        if (!sdlWin)
        {
            SDLCalls::Start(0, 0);
            sdlWin = SDLCalls::GetWindow();
        }
        if (!sdlWin)
            return 0;

        SDL_SetWindowSize(sdlWin, width, height);
        SDL_SetWindowPosition(sdlWin, x, y);
        return RegisterWindow(sdlWin);
    }

    Window RegisterWindow(SDL_Window *sdlWin)
    {
        auto existing = reverseWindowMap.find(sdlWin);
        if (existing != reverseWindowMap.end())
            return existing->second;

        Window xid = ++mainWindowID;
        windowMap[xid] = sdlWin;
        reverseWindowMap[sdlWin] = xid;
        return xid;
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

    Atom InternAtom(const char *name, int only_if_exists)
    {
        if (!name)
            return 0;

        auto it = atomNameToId.find(name);
        if (it != atomNameToId.end())
            return it->second;

        if (only_if_exists)
            return 0;

        Atom atom = nextAtomId++;
        atomNameToId[name] = atom;
        return atom;
    }

    Colormap CreateColormap(Window win)
    {
        SDL_Window *sdlWin = GetSDLWindow(win);
        if (!sdlWin)
            return nextColormapId++;

        auto existing = windowColormapMap.find(sdlWin);
        if (existing != windowColormapMap.end())
            return existing->second;

        Colormap cmap = nextColormapId++;
        windowColormapMap[sdlWin] = cmap;
        return cmap;
    }

    void PushEvent(const XEvent &ev)
    {
        std::lock_guard<std::mutex> lock(eventQueueMutex);
        eventQueue.push_back(ev);
    }

    bool PopEvent(XEvent *outEv)
    {
        std::lock_guard<std::mutex> lock(eventQueueMutex);
        if (eventQueue.empty())
            return false;
        *outEv = eventQueue.front();
        eventQueue.pop_front();
        return true;
    }

    bool HasEvents()
    {
        std::lock_guard<std::mutex> lock(eventQueueMutex);
        return !eventQueue.empty();
    }

    void PumpEvents()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            TranslateAndPushEvent(e);
        }
    }

  private:
    void TranslateAndPushEvent(const SDL_Event &e)
    {
        XEvent xev = {0};
        Window winID = GetXID(SDL_GetWindowFromID(e.window.windowID));

        switch (e.type)
        {
            case SDL_EVENT_QUIT:
                log_info("X11Bridge: SDL_EVENT_QUIT");
                exit(0);
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                xev.xkey.type = (e.type == SDL_EVENT_KEY_DOWN) ? KeyPress : KeyRelease;
                xev.xkey.window = winID;
                xev.xkey.keycode = e.key.scancode;
                xev.xkey.time = (Time)e.key.timestamp;
                xev.xkey.display = XServerState::Instance().GetDisplay();
                PushEvent(xev);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                xev.xmotion.type = MotionNotify;
                xev.xmotion.window = winID;
                xev.xmotion.x = (int)e.motion.x;
                xev.xmotion.y = (int)e.motion.y;
                xev.xmotion.time = (Time)e.motion.timestamp;
                xev.xmotion.display = XServerState::Instance().GetDisplay();
                PushEvent(xev);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                xev.xbutton.type = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? ButtonPress : ButtonRelease;
                xev.xbutton.window = winID;
                xev.xbutton.button = e.button.button;
                xev.xbutton.x = (int)e.button.x;
                xev.xbutton.y = (int)e.button.y;
                xev.xbutton.time = (Time)e.button.timestamp;
                xev.xbutton.display = XServerState::Instance().GetDisplay();
                PushEvent(xev);
                break;
        }
    }

    bool sdlInitialized = false;
    Window mainWindowID = 1;
    Atom nextAtomId = 1;
    Colormap nextColormapId = 1;
    std::map<Window, SDL_Window *> windowMap;
    std::map<SDL_Window *, Window> reverseWindowMap;
    std::map<std::string, Atom> atomNameToId;
    std::map<SDL_Window *, Colormap> windowColormapMap;
    std::deque<XEvent> eventQueue;
    std::mutex eventQueueMutex;
};

namespace
{
bool ParseHexColor(const char *spec, XColor *color)
{
    if (!spec || spec[0] != '#' || !color)
        return false;

    size_t len = SDL_strlen(spec + 1);
    const char *hex = spec + 1;
    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    };

    auto parseComponent = [&](int digits, unsigned short &out) -> bool {
        int value = 0;
        for (int i = 0; i < digits; ++i)
        {
            int v = hexValue(hex[i]);
            if (v < 0)
                return false;
            value = (value << 4) | v;
        }
        if (digits == 1)
            out = static_cast<unsigned short>(value * 0x1111);
        else if (digits == 2)
            out = static_cast<unsigned short>(value * 0x0101);
        else
            out = static_cast<unsigned short>(value);
        hex += digits;
        return true;
    };

    if (len == 3)
    {
        if (!parseComponent(1, color->red) || !parseComponent(1, color->green) || !parseComponent(1, color->blue))
            return false;
    }
    else if (len == 6)
    {
        if (!parseComponent(2, color->red) || !parseComponent(2, color->green) || !parseComponent(2, color->blue))
            return false;
    }
    else if (len == 12)
    {
        if (!parseComponent(4, color->red) || !parseComponent(4, color->green) || !parseComponent(4, color->blue))
            return false;
    }
    else
    {
        return false;
    }

    color->flags = 0x07;
    return true;
}

bool ParseNamedColor(const char *spec, XColor *color)
{
    if (!spec || !color)
        return false;

    if (SDL_strcasecmp(spec, "black") == 0)
    {
        color->red = 0;
        color->green = 0;
        color->blue = 0;
    }
    else if (SDL_strcasecmp(spec, "white") == 0)
    {
        color->red = 0xFFFF;
        color->green = 0xFFFF;
        color->blue = 0xFFFF;
    }
    else if (SDL_strcasecmp(spec, "red") == 0)
    {
        color->red = 0xFFFF;
        color->green = 0;
        color->blue = 0;
    }
    else if (SDL_strcasecmp(spec, "green") == 0)
    {
        color->red = 0;
        color->green = 0xFFFF;
        color->blue = 0;
    }
    else if (SDL_strcasecmp(spec, "blue") == 0)
    {
        color->red = 0;
        color->green = 0;
        color->blue = 0xFFFF;
    }
    else
    {
        return false;
    }

    color->flags = 0x07;
    return true;
}
} // namespace

// --------------------------------------------------------------------------
// X11Bridge Implementation
// --------------------------------------------------------------------------

int X11Bridge::InitThreads()
{
    return 1;
}

Display *X11Bridge::OpenDisplay(const char *name)
{
    XServerState::Instance().Initialize();

    if (!SDLCalls::GetWindow())
        SDLCalls::Start(nullptr, nullptr);

    Display *iVar1 = InitFakeDisplay();

    log_info("XOpenDisplay(\"%s\") = %p", name ? name : "", (void *)iVar1);
    return iVar1;
}

int X11Bridge::CloseDisplay(Display *dpy)
{
    (void)dpy;
    return 0;
}

Window X11Bridge::XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height,
                                unsigned int border_width, int depth, unsigned int class_type, void *visual, unsigned long valuemask,
                                XSetWindowAttributes *attributes)
{
    log_info("XCreateWindow: %ux%u", width, height);
    return XServerState::Instance().CreateVirtualWindow(x, y, width, height);
}

int X11Bridge::DestroyWindow(Display *dpy, Window win)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_HideWindow(sdlWin);
    return 1;
}

void X11Bridge::MapWindow(Display *dpy, Window win)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_ShowWindow(sdlWin);
}

int X11Bridge::MoveWindow(Display *dpy, Window win, int x, int y)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_SetWindowPosition(sdlWin, x, y);
    return 1;
}

int X11Bridge::ResizeWindow(Display *dpy, Window win, unsigned int width, unsigned int height)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_SetWindowSize(sdlWin, width, height);
    return 1;
}

int X11Bridge::Sync(Display *dpy, int discard)
{
    return 1;
}
int X11Bridge::Flush(Display *dpy)
{
    return 1;
}

void X11Bridge::LockDisplay(Display *dpy)
{
}
void X11Bridge::UnlockDisplay(Display *dpy)
{
}
void X11Bridge::SetErrorHandler(void *handler)
{
}
int X11Bridge::GetErrorText(Display *dpy, int code, char *buffer, int length)
{
    if (buffer && length > 0)
        buffer[0] = '\0';
    return 0;
}

// --- Event Handling ---

int X11Bridge::Pending(Display *dpy)
{
    XServerState::Instance().PumpEvents();
    return XServerState::Instance().HasEvents() ? 1 : 0;
}

void X11Bridge::NextEvent(Display *dpy, XEvent *event_return)
{
    while (true)
    {
        XServerState::Instance().PumpEvents();
        if (XServerState::Instance().PopEvent(event_return))
            return;
        SDL_Delay(1);
    }
}

int X11Bridge::CheckTypedEvent(Display *dpy, int type, XEvent *event_return)
{
    XServerState::Instance().PumpEvents();
    return 0; // Simplified
}

int X11Bridge::SelectInput(Display *dpy, Window win, long event_mask)
{
    return 0;
}
int X11Bridge::SetCloseDownMode(Display *dpy, int close_mode)
{
    return 1;
}

int X11Bridge::SetInputFocus(Display *dpy, Window win, int revert_to, Time time)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_RaiseWindow(sdlWin);
    return 1;
}

int X11Bridge::TranslateCoordinates(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, int *dest_x_return, int *dest_y_return,
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

int X11Bridge::QueryPointer(Display *dpy, Window win, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return,
                            int *win_x_return, int *win_y_return, unsigned int *mask_return)
{
    float mx, my;
    uint32_t mask = SDL_GetGlobalMouseState(&mx, &my);

    if (root_x_return)
        *root_x_return = (int)mx;
    if (root_y_return)
        *root_y_return = (int)my;

    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
    {
        int wx, wy;
        SDL_GetWindowPosition(sdlWin, &wx, &wy);
        if (win_x_return)
            *win_x_return = (int)mx - wx;
        if (win_y_return)
            *win_y_return = (int)my - wy;
    }

    if (root_return)
        *root_return = g_fakeScreen.root;
    if (child_return)
        *child_return = 0;
    if (mask_return)
        *mask_return = mask;
    return 1;
}

void X11Bridge::WarpPointer(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width,
                            unsigned int src_height, int dest_x, int dest_y)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(dest_w);
    if (!sdlWin)
        sdlWin = SDLCalls::GetWindow();
    if (sdlWin)
        SDL_WarpMouseInWindow(sdlWin, (float)dest_x, (float)dest_y);
}

void X11Bridge::GrabPointer(Display *dpy, Window win, int owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode,
                            Window confine_to, Cursor cursor, Time time)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
        SDL_SetWindowMouseGrab(sdlWin, true);
}

void X11Bridge::UngrabPointer(Display *dpy, Time time)
{
    if (SDLCalls::GetWindow())
        SDL_SetWindowMouseGrab(SDLCalls::GetWindow(), false);
}

int X11Bridge::LookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return,
                            XComposeStatus *status_in_out)
{
    if (!event_struct)
        return 0;

    SDL_Keycode keycode = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(event_struct->keycode), SDL_KMOD_NONE, false);
    if (keysym_return)
        *keysym_return = static_cast<KeySym>(keycode);

    if (!buffer_return || bytes_buffer <= 0)
        return 0;

    if (keycode >= 0x20 && keycode <= 0x7E)
    {
        buffer_return[0] = static_cast<char>(keycode);
        return 1;
    }

    if (bytes_buffer > 0)
        buffer_return[0] = '\0';
    return 0;
}

int X11Bridge::GrabKeyboard(Display *dpy, Window win, int owner_events, int pointer_mode, int keyboard_mode, Time time)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
    {
        SDL_SetWindowKeyboardGrab(sdlWin, true);
        return 0; // Success
    }
    return 1;
}

int X11Bridge::UngrabKeyboard(Display *dpy, Time time)
{
    if (SDLCalls::GetWindow())
        SDL_SetWindowKeyboardGrab(SDLCalls::GetWindow(), false);
    return 0;
}

int X11Bridge::AutoRepeatOn(Display *dpy)
{
    return 1;
}

// --- Attributes & Properties ---

int X11Bridge::GetWindowAttributes(Display *dpy, Window win, XWindowAttributes *attr)
{
    if (!attr)
        return 0;
    memset(attr, 0, sizeof(XWindowAttributes));
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin)
    {
        int w, h, x, y;
        SDL_GetWindowSize(sdlWin, &w, &h);
        SDL_GetWindowPosition(sdlWin, &x, &y);
        attr->x = x;
        attr->y = y;
        attr->width = w;
        attr->height = h;
        attr->depth = 24;
        attr->visual = (void *)1;
        attr->root = g_fakeScreen.root;
        attr->map_state = 2; // IsViewable
    }
    return 1;
}

int X11Bridge::GetGeometry(Display *dpy, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return,
                           unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow((Window)d);
    if (sdlWin)
    {
        int w, h, x, y;
        SDL_GetWindowSize(sdlWin, &w, &h);
        SDL_GetWindowPosition(sdlWin, &x, &y);
        if (x_return)
            *x_return = x;
        if (y_return)
            *y_return = y;
        if (width_return)
            *width_return = w;
        if (height_return)
            *height_return = h;
        if (depth_return)
            *depth_return = 24;
        if (border_width_return)
            *border_width_return = 0;
        if (root_return)
            *root_return = g_fakeScreen.root;
    }
    return 1;
}

Atom X11Bridge::InternAtom(Display *dpy, const char *name, int only_if_exists)
{
    return XServerState::Instance().InternAtom(name, only_if_exists);
}

int X11Bridge::StoreName(Display *dpy, Window win, const char *name)
{
    SDL_Window *sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin && name)
        SDL_SetWindowTitle(sdlWin, name);
    return 0;
}

Colormap X11Bridge::CreateColormap(Display *dpy, Window w, void *visual, int alloc)
{
    return XServerState::Instance().CreateColormap(w);
}

int X11Bridge::ParseColor(Display *dpy, Colormap colormap, const char *spec, XColor *color_return)
{
    if (!color_return || !spec)
        return 0;

    color_return->pixel = 0;
    color_return->red = 0;
    color_return->green = 0;
    color_return->blue = 0;
    color_return->flags = 0;
    color_return->pad = 0;

    if (ParseHexColor(spec, color_return))
        return 1;
    if (ParseNamedColor(spec, color_return))
        return 1;

    return 0;
}

int X11Bridge::ChangeProperty(Display *dpy, Window win, Atom property, Atom type, int format, int mode, const unsigned char *data,
                              int nelements)
{
    return 1;
}

int X11Bridge::SetWMProtocols(Display *dpy, Window win, Atom *protocols, int count)
{
    return 1;
}

int X11Bridge::SetWMProperties(Display *dpy, Window win, XTextProperty *window_name, XTextProperty *icon_name, char **argv, int argc,
                               XSizeHints *normal_hints, XWMHints *wm_hints, XClassHint *class_hints)
{
    return 1;
}

int X11Bridge::StringListToTextProperty(char **list, int count, XTextProperty *text_prop_return)
{
    if (text_prop_return && count > 0 && list && list[0])
    {
        size_t len = SDL_strlen(list[0]);
        text_prop_return->value = (unsigned char *)SDL_malloc(len + 1);
        if (text_prop_return->value)
            SDL_strlcpy((char *)text_prop_return->value, list[0], len + 1);
        text_prop_return->encoding = 31;
        text_prop_return->format = 8;
        text_prop_return->nitems = len;
        return 1;
    }
    return 0;
}

void X11Bridge::Free(void *data)
{
    if (data)
        SDL_free(data);
}

// --- Pixmaps & Cursors ---

Pixmap X11Bridge::CreatePixmapFromBitmapData(Display *dpy, Drawable drawable, char *data, unsigned int width, unsigned int height,
                                             unsigned long fg, unsigned long bg, unsigned int depth)
{
    return 2001;
}

Cursor X11Bridge::CreatePixmapCursor(Display *dpy, Pixmap source, Pixmap mask, void *foreground_color, void *background_color,
                                     unsigned int x, unsigned int y)
{
    return 1001;
}

int X11Bridge::FreePixmap(Display *dpy, Pixmap pixmap)
{
    return 1;
}
int X11Bridge::FreeCursor(Display *dpy, Cursor cursor)
{
    return 1;
}
void X11Bridge::DefineCursor(Display *dpy, Window w, Cursor c)
{
}

// --- XF86VidMode Extension (Stubs) ---

int X11Bridge::VidModeQueryExtension(Display *dpy, int *event_base_return, int *error_base_return)
{
    if (event_base_return)
        *event_base_return = 0;
    if (error_base_return)
        *error_base_return = 0;
    return 1;
}

int X11Bridge::VidModeGetAllModeLines(Display *dpy, int screen, int *modecount_return, void ***modesinfo_return)
{
    if (modecount_return)
        *modecount_return = 0;
    if (modesinfo_return)
        *modesinfo_return = nullptr;
    return 1;
}

int X11Bridge::VidModeGetModeLine(Display *dpy, int screen, int *dotclock_return, void *modeline)
{
    if (dotclock_return)
        *dotclock_return = 0;
    return 1;
}

int X11Bridge::VidModeSwitchToMode(Display *dpy, int screen, void *modeline)
{
    log_info("VidModeSwitchToMode (Stub)");
    return 1;
}

int X11Bridge::VidModeSetViewPort(Display *dpy, int screen, int x, int y)
{
    return 1;
}

int X11Bridge::VidModeGetViewPort(Display *dpy, int screen, int *x_return, int *y_return)
{
    if (x_return)
        *x_return = 0;
    if (y_return)
        *y_return = 0;
    return 1;
}

int X11Bridge::VidModeLockModeSwitch(Display *dpy, int screen, int lock)
{
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