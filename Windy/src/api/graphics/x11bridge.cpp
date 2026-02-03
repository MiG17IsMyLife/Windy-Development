#define _CRT_SECURE_NO_WARNINGS

#include "X11Bridge.h"
#include "../../core/log.h"
#include <SDL3/SDL.h>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <cstring>
#include <cstdlib>

// Windows API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// --------------------------------------------------------------------------
// Global State Definitions
// --------------------------------------------------------------------------
SDL_Window* g_sdlWindow = nullptr;
int g_windowWidth = 1360;
int g_windowHeight = 768;
// --------------------------------------------------------------------------
// Internal X Server State Manager
// --------------------------------------------------------------------------
class XServerState {
public:
    static XServerState& Instance() {
        static XServerState instance;
        return instance;
    }

    void Initialize() {
        if (!sdlInitialized) {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
                log_error("X11Bridge: SDL_Init failed: %s", SDL_GetError());
            }
            else {
                sdlInitialized = true;
                log_info("X11Bridge: SDL Subsystem Initialized");
            }
        }
    }

    Window CreateVirtualWindow(int width, int height) {
        if (g_sdlWindow) {
            SDL_SetWindowSize(g_sdlWindow, width, height);
            g_windowWidth = width;
            g_windowHeight = height;
            RegisterWindow(g_sdlWindow);
            return mainWindowID;
        }

        g_sdlWindow = SDL_CreateWindow("Windy - SEGA Lindbergh Emulator for Windows", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!g_sdlWindow) {
            log_error("X11Bridge: Failed to create SDL Window: %s", SDL_GetError());
            return 0;
        }

        g_windowWidth = width;
        g_windowHeight = height;
        RegisterWindow(g_sdlWindow);
        return mainWindowID;
    }

    void RegisterWindow(SDL_Window* sdlWin) {
        if (reverseWindowMap.find(sdlWin) == reverseWindowMap.end()) {
            mainWindowID = 1;
            windowMap[mainWindowID] = sdlWin;
            reverseWindowMap[sdlWin] = mainWindowID;
        }
    }

    SDL_Window* GetSDLWindow(Window win) {
        if (windowMap.find(win) != windowMap.end()) {
            return windowMap[win];
        }
        return g_sdlWindow;
    }

    Window GetXID(SDL_Window* win) {
        if (reverseWindowMap.find(win) != reverseWindowMap.end()) {
            return reverseWindowMap[win];
        }
        return 1;
    }

    void PushEvent(const XEvent& ev) {
        std::lock_guard<std::mutex> lock(eventQueueMutex);
        eventQueue.push_back(ev);
    }

    bool PopEvent(XEvent* outEv) {
        std::lock_guard<std::mutex> lock(eventQueueMutex);
        if (eventQueue.empty()) return false;
        *outEv = eventQueue.front();
        eventQueue.pop_front();
        return true;
    }

    bool HasEvents() {
        std::lock_guard<std::mutex> lock(eventQueueMutex);
        return !eventQueue.empty();
    }

    void PumpEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            TranslateAndPushEvent(e);
        }
    }

private:
    void TranslateAndPushEvent(const SDL_Event& e) {
        XEvent xev = { 0 };
        Window winID = GetXID(SDL_GetWindowFromID(e.window.windowID));

        switch (e.type) {
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
            xev.xkey.display = (Display*)0x1111;
            PushEvent(xev);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            xev.xmotion.type = MotionNotify;
            xev.xmotion.window = winID;
            xev.xmotion.x = (int)e.motion.x;
            xev.xmotion.y = (int)e.motion.y;
            xev.xmotion.time = (Time)e.motion.timestamp;
            xev.xmotion.display = (Display*)0x1111;
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
            xev.xbutton.display = (Display*)0x1111;
            PushEvent(xev);
            break;
        }
    }

    bool sdlInitialized = false;
    Window mainWindowID = 1;
    std::map<Window, SDL_Window*> windowMap;
    std::map<SDL_Window*, Window> reverseWindowMap;
    std::deque<XEvent> eventQueue;
    std::mutex eventQueueMutex;
};

// --------------------------------------------------------------------------
// X11Bridge Implementation
// --------------------------------------------------------------------------

int X11Bridge::InitThreads() { return 1; }

Display* X11Bridge::OpenDisplay(const char* name) {
    log_info("XOpenDisplay(\"%s\")", name ? name : "NULL");
    XServerState::Instance().Initialize();
    return (Display*)0x1111;
}

int X11Bridge::CloseDisplay(Display* dpy) { return 0; }

Window X11Bridge::XCreateWindow(Display* dpy, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_type, void* visual, unsigned long valuemask, XSetWindowAttributes* attributes) {
    log_info("XCreateWindow: %ux%u", width, height);
    return XServerState::Instance().CreateVirtualWindow(width, height);
}

int X11Bridge::DestroyWindow(Display* dpy, Window win) { return 1; }

void X11Bridge::MapWindow(Display* dpy, Window win) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) SDL_ShowWindow(sdlWin);
}

int X11Bridge::MoveWindow(Display* dpy, Window win, int x, int y) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) SDL_SetWindowPosition(sdlWin, x, y);
    return 1;
}

int X11Bridge::ResizeWindow(Display* dpy, Window win, unsigned int width, unsigned int height) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) SDL_SetWindowSize(sdlWin, width, height);
    return 1;
}

int X11Bridge::Sync(Display* dpy, int discard) { return 1; }
int X11Bridge::Flush(Display* dpy) { return 1; }

void X11Bridge::LockDisplay(Display* dpy) {}
void X11Bridge::UnlockDisplay(Display* dpy) {}
void X11Bridge::SetErrorHandler(void* handler) {}
int X11Bridge::GetErrorText(Display* dpy, int code, char* buffer, int length) {
    if (buffer && length > 0) buffer[0] = '\0';
    return 0;
}

// --- Event Handling ---

int X11Bridge::Pending(Display* dpy) {
    XServerState::Instance().PumpEvents();
    return XServerState::Instance().HasEvents() ? 1 : 0;
}

void X11Bridge::NextEvent(Display* dpy, XEvent* event_return) {
    while (true) {
        XServerState::Instance().PumpEvents();
        if (XServerState::Instance().PopEvent(event_return)) return;
        SDL_Delay(1);
    }
}

int X11Bridge::CheckTypedEvent(Display* dpy, int type, XEvent* event_return) {
    XServerState::Instance().PumpEvents();
    return 0; // Simplified
}

int X11Bridge::SelectInput(Display* dpy, Window win, long event_mask) { return 0; }
int X11Bridge::SetCloseDownMode(Display* dpy, int close_mode) { return 1; }

int X11Bridge::SetInputFocus(Display* dpy, Window win, int revert_to, Time time) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) SDL_RaiseWindow(sdlWin);
    return 1;
}

// --- Input (Mouse/Keyboard) ---

int X11Bridge::QueryPointer(Display* dpy, Window win, Window* root_return, Window* child_return, int* root_x_return, int* root_y_return, int* win_x_return, int* win_y_return, unsigned int* mask_return) {
    float mx, my;
    uint32_t mask = SDL_GetGlobalMouseState(&mx, &my);

    if (root_x_return) *root_x_return = (int)mx;
    if (root_y_return) *root_y_return = (int)my;

    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) {
        int wx, wy;
        SDL_GetWindowPosition(sdlWin, &wx, &wy);
        if (win_x_return) *win_x_return = (int)mx - wx;
        if (win_y_return) *win_y_return = (int)my - wy;
    }

    if (mask_return) *mask_return = mask;
    return 1;
}

void X11Bridge::WarpPointer(Display* dpy, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width, unsigned int src_height, int dest_x, int dest_y) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(dest_w);
    if (sdlWin) SDL_WarpMouseInWindow(sdlWin, (float)dest_x, (float)dest_y);
}

void X11Bridge::GrabPointer(Display* dpy, Window win, int owner_events, unsigned int event_mask, int pointer_mode, int keyboard_mode, Window confine_to, Cursor cursor, Time time) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) SDL_SetWindowMouseGrab(sdlWin, true);
}

void X11Bridge::UngrabPointer(Display* dpy, Time time) {
    if (g_sdlWindow) SDL_SetWindowMouseGrab(g_sdlWindow, false);
}

int X11Bridge::GrabKeyboard(Display* dpy, Window win, int owner_events, int pointer_mode, int keyboard_mode, Time time) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) {
        SDL_SetWindowKeyboardGrab(sdlWin, true);
        return 0; // Success
    }
    return 1;
}

int X11Bridge::UngrabKeyboard(Display* dpy, Time time) {
    if (g_sdlWindow) SDL_SetWindowKeyboardGrab(g_sdlWindow, false);
    return 0;
}

int X11Bridge::AutoRepeatOn(Display* dpy) { return 1; }

// --- Attributes & Properties ---

int X11Bridge::GetWindowAttributes(Display* dpy, Window win, XWindowAttributes* attr) {
    if (!attr) return 0;
    memset(attr, 0, sizeof(XWindowAttributes));
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin) {
        int w, h, x, y;
        SDL_GetWindowSize(sdlWin, &w, &h);
        SDL_GetWindowPosition(sdlWin, &x, &y);
        attr->x = x; attr->y = y;
        attr->width = w; attr->height = h;
        attr->depth = 24;
        attr->visual = (void*)1;
        attr->map_state = 2; // IsViewable
    }
    return 1;
}

int X11Bridge::GetGeometry(Display* dpy, Drawable d, Window* root_return, int* x_return, int* y_return, unsigned int* width_return, unsigned int* height_return, unsigned int* border_width_return, unsigned int* depth_return) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow((Window)d);
    if (sdlWin) {
        int w, h, x, y;
        SDL_GetWindowSize(sdlWin, &w, &h);
        SDL_GetWindowPosition(sdlWin, &x, &y);
        if (x_return) *x_return = x;
        if (y_return) *y_return = y;
        if (width_return) *width_return = w;
        if (height_return) *height_return = h;
        if (depth_return) *depth_return = 24;
        if (border_width_return) *border_width_return = 0;
    }
    return 1;
}

Atom X11Bridge::InternAtom(Display* dpy, const char* name, int only_if_exists) {
    unsigned long hash = 0;
    if (name) {
        for (const char* p = name; *p; p++) hash = hash * 33 + *p;
    }
    return hash;
}

int X11Bridge::StoreName(Display* dpy, Window win, const char* name) {
    SDL_Window* sdlWin = XServerState::Instance().GetSDLWindow(win);
    if (sdlWin && name) SDL_SetWindowTitle(sdlWin, name);
    return 0;
}

int X11Bridge::ChangeProperty(Display* dpy, Window win, Atom property, Atom type, int format, int mode, const unsigned char* data, int nelements) { return 1; }

int X11Bridge::SetWMProtocols(Display* dpy, Window win, Atom* protocols, int count) { return 1; }

int X11Bridge::SetWMProperties(Display* dpy, Window win, XTextProperty* window_name, XTextProperty* icon_name, char** argv, int argc, XSizeHints* normal_hints, XWMHints* wm_hints, XClassHint* class_hints) { return 1; }

int X11Bridge::StringListToTextProperty(char** list, int count, XTextProperty* text_prop_return) {
    if (text_prop_return && count > 0 && list && list[0]) {
        size_t len = strlen(list[0]);
        // Note: In a real X11 impl, XFree should free this. We use _aligned_malloc in original, here just malloc.
        text_prop_return->value = (unsigned char*)malloc(len + 1);
        if (text_prop_return->value) strcpy((char*)text_prop_return->value, list[0]);
        text_prop_return->encoding = 31;
        text_prop_return->format = 8;
        text_prop_return->nitems = len;
        return 1;
    }
    return 0;
}

void X11Bridge::Free(void* data) {
    if (data) free(data);
}

// --- Pixmaps & Cursors ---

Pixmap X11Bridge::CreatePixmapFromBitmapData(Display* dpy, Drawable drawable, char* data, unsigned int width, unsigned int height, unsigned long fg, unsigned long bg, unsigned int depth) {
    return 2001;
}

Cursor X11Bridge::CreatePixmapCursor(Display* dpy, Pixmap source, Pixmap mask, void* foreground_color, void* background_color, unsigned int x, unsigned int y) {
    return 1001;
}

int X11Bridge::FreePixmap(Display* dpy, Pixmap pixmap) { return 1; }
int X11Bridge::FreeCursor(Display* dpy, Cursor cursor) { return 1; }
void X11Bridge::DefineCursor(Display* dpy, Window w, Cursor c) {}

// --- XF86VidMode Extension (Stubs) ---

int X11Bridge::VidModeQueryExtension(Display* dpy, int* event_base_return, int* error_base_return) {
    if (event_base_return) *event_base_return = 0;
    if (error_base_return) *error_base_return = 0;
    return 1;
}

int X11Bridge::VidModeGetAllModeLines(Display* dpy, int screen, int* modecount_return, void*** modesinfo_return) {
    if (modecount_return) *modecount_return = 0;
    if (modesinfo_return) *modesinfo_return = nullptr;
    return 1;
}

int X11Bridge::VidModeGetModeLine(Display* dpy, int screen, int* dotclock_return, void* modeline) {
    if (dotclock_return) *dotclock_return = 0;
    return 1;
}

int X11Bridge::VidModeSwitchToMode(Display* dpy, int screen, void* modeline) {
    log_info("VidModeSwitchToMode (Stub)");
    return 1;
}

int X11Bridge::VidModeSetViewPort(Display* dpy, int screen, int x, int y) { return 1; }

int X11Bridge::VidModeGetViewPort(Display* dpy, int screen, int* x_return, int* y_return) {
    if (x_return) *x_return = 0;
    if (y_return) *y_return = 0;
    return 1;
}

int X11Bridge::VidModeLockModeSwitch(Display* dpy, int screen, int lock) { return 1; }

// Integration
SDL_Window* X11Bridge::GetSDLWindow(Window win) {
    return XServerState::Instance().GetSDLWindow(win);
}

Window X11Bridge::GetXWindowFromSDL(SDL_Window* sdlWin) {
    return XServerState::Instance().GetXID(sdlWin);
}