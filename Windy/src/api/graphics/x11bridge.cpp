#define _CRT_SECURE_NO_WARNINGS

#include "X11Bridge.h"
#include "../src/core/log.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <SDL3/SDL.h>

extern SDL_Window* g_sdlWindow;
extern int g_windowWidth;
extern int g_windowHeight;
extern void CreateSDLWindowIfNeeded();

HWND X11Bridge::m_hWnd = NULL;

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------
static void* x_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void* p = _aligned_malloc(total, 16);
    if (p) memset(p, 0, total);
    return p;
}
static void x_free(void* p) { if (p) _aligned_free(p); }

// X11 Structures (Stub)
struct DummyDisplay { char pad[4096]; };
struct DummyXEvent { int type; unsigned long serial; int send_event; unsigned long display; unsigned long window; int pad[20]; };
struct DummyXSizeHints { long flags; int x, y, width, height, min_width, min_height, max_width, max_height, width_inc, height_inc; struct { int x; int y; } min_aspect, max_aspect; int base_width, base_height; int win_gravity; };
struct DummyXWMHints { long flags; int input; int initial_state; unsigned long icon_pixmap; unsigned long icon_window; int icon_x, icon_y; unsigned long icon_mask; unsigned long window_group; };
struct DummyXClassHint { char* res_name; char* res_class; };
struct DummyTextProperty { unsigned char* value; unsigned long encoding; int format; unsigned long nitems; };

// ---------------------------------------------------------
// Implementation
// ---------------------------------------------------------

LRESULT CALLBACK X11Bridge::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int X11Bridge::InitThreads() {
    log_trace("XInitThreads()");
    return 1;
}

void* X11Bridge::OpenDisplay(const char* name) {
    log_info("XOpenDisplay(\"%s\")", name ? name : "NULL");
    CreateSDLWindowIfNeeded();
    return x_calloc(1, sizeof(DummyDisplay));
}

int X11Bridge::CloseDisplay(void* dpy) {
    log_debug("XCloseDisplay(%p)", dpy);
    x_free(dpy);
    return 0;
}

unsigned long X11Bridge::XCreateWindow(void* dpy, unsigned long parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_type, void* visual, unsigned long valuemask, void* attributes) {
    log_info("XCreateWindow: %ux%u at (%d,%d)", width, height, x, y);

    if (width > 0) g_windowWidth = width;
    if (height > 0) g_windowHeight = height;

    CreateSDLWindowIfNeeded();
    if (g_sdlWindow) {
        SDL_SetWindowSize(g_sdlWindow, g_windowWidth, g_windowHeight);
        SDL_ShowWindow(g_sdlWindow);
    }

    return 1; // Dummy Window ID (Must correspond to GLXBridge)
}

void X11Bridge::MapWindow(void* dpy, unsigned long win) {
    log_trace("XMapWindow(%p, %lu)", dpy, win);
    if (g_sdlWindow) SDL_ShowWindow(g_sdlWindow);
}

int X11Bridge::Pending(void* dpy) {
    if (SDL_HasEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST)) {
        return 1;
    }
    return 0;
}

void X11Bridge::NextEvent(void* dpy, void* event_return) {
    DummyXEvent* ev = (DummyXEvent*)event_return;
    memset(ev, 0, sizeof(DummyXEvent));

    SDL_Event e;
    if (SDL_WaitEventTimeout(&e, 10)) {
    }

    static bool mapped = false;
    if (!mapped) {
        ev->type = 19; // MapNotify
        ev->window = 1;
        mapped = true;
    }
    else {
        ev->type = 0;
    }
}

int X11Bridge::Sync(void* dpy, int discard) { return 0; }
int X11Bridge::Flush(void* dpy) { return 0; }
void X11Bridge::Free(void* data) { x_free(data); }
void X11Bridge::SetInputFocus(void* dpy, unsigned long win, int revert, unsigned long time) {}
void X11Bridge::SetErrorHandler(void* handler) {}

unsigned long X11Bridge::InternAtom(void* dpy, const char* name, int only_if_exists) {
    log_trace("XInternAtom(\"%s\")", name);
    unsigned long hash = 0;
    if (name) { for (const char* p = name; *p; p++) hash = hash * 31 + *p; }
    return hash ? hash : 0x123;
}

void X11Bridge::StoreName(void* dpy, unsigned long win, const char* name) {
    log_debug("XStoreName: \"%s\"", name);
    if (g_sdlWindow && name) SDL_SetWindowTitle(g_sdlWindow, name);
}

int X11Bridge::LookupString(void* event, char* buffer, int n, void* k, void* s) { return 0; }
void X11Bridge::DefineCursor(void* d, unsigned long w, unsigned long c) {}
void X11Bridge::WarpPointer(void* d, unsigned long s, unsigned long de, int sx, int sy, unsigned int sw, unsigned int sh, int dx, int dy) {}
void X11Bridge::GrabKeyboard(void* d, unsigned long w, int o, int p, int k, unsigned long t) {}
void X11Bridge::GrabPointer(void* d, unsigned long w, int o, unsigned int m, int p, int k, unsigned long c, unsigned long cu, unsigned long t) {}
void X11Bridge::UngrabKeyboard(void* d, unsigned long t) {}
void X11Bridge::UngrabPointer(void* d, unsigned long t) {}

int X11Bridge::SetWMProtocols(void* dpy, unsigned long win, unsigned long* protocols, int count) { return 1; }
int X11Bridge::GetWindowAttributes(void* dpy, unsigned long win, void* attr) { return 1; }
int X11Bridge::SelectInput(void* dpy, unsigned long win, long event_mask) { return 1; }
unsigned long X11Bridge::CreateColormap(void* dpy, unsigned long win, void* visual, int alloc) { return 0xCC; }
void X11Bridge::SetWMName(void* dpy, unsigned long win, void* text_prop) {}
int X11Bridge::SetWMHints(void* dpy, unsigned long win, void* hints) { return 1; }
int X11Bridge::SetWMProperties(void* dpy, unsigned long win, void* window_name, void* icon_name, char** argv, int argc, void* normal_hints, void* wm_hints, void* class_hints) { return 1; }
int X11Bridge::ChangeProperty(void* dpy, unsigned long win, unsigned long property, unsigned long type, int format, int mode, const unsigned char* data, int nelements) { return 1; }

int X11Bridge::GetGeometry(void* dpy, unsigned long drawable, unsigned long* root_return, int* x_return, int* y_return, unsigned int* width_return, unsigned int* height_return, unsigned int* border_width_return, unsigned int* depth_return) {
    if (width_return) *width_return = g_windowWidth;
    if (height_return) *height_return = g_windowHeight;
    if (depth_return) *depth_return = 24;
    return 1;
}

int X11Bridge::TranslateCoordinates(void* dpy, unsigned long src_w, unsigned long dest_w, int src_x, int src_y, int* dest_x_return, int* dest_y_return, unsigned long* child_return) {
    if (dest_x_return) *dest_x_return = src_x;
    if (dest_y_return) *dest_y_return = src_y;
    return 1;
}
int X11Bridge::QueryPointer(void* dpy, unsigned long win, unsigned long* root_return, unsigned long* child_return, int* root_x_return, int* root_y_return, int* win_x_return, int* win_y_return, unsigned int* mask_return) {
    if (root_x_return) *root_x_return = 0;
    if (root_y_return) *root_y_return = 0;
    if (win_x_return) *win_x_return = 0;
    if (win_y_return) *win_y_return = 0;
    return 1;
}

int X11Bridge::MoveWindow(void* dpy, unsigned long win, int x, int y) { return 1; }
int X11Bridge::DestroyWindow(void* dpy, unsigned long win) { return 1; }
void X11Bridge::LockDisplay(void* dpy) {}
void X11Bridge::UnlockDisplay(void* dpy) {}
int X11Bridge::FreePixmap(void* dpy, unsigned long pixmap) { return 1; }
int X11Bridge::FreeCursor(void* dpy, unsigned long cursor) { return 1; }
unsigned long X11Bridge::CreatePixmapCursor(void* dpy, unsigned long source, unsigned long mask, void* foreground_color, void* background_color, unsigned int x, unsigned int y) { return 0x1; }
unsigned long X11Bridge::CreatePixmapFromBitmapData(void* dpy, unsigned long drawable, char* data, unsigned int width, unsigned int height, unsigned long fg, unsigned long bg, unsigned int depth) { return 0x2; }

int X11Bridge::ParseColor(void* dpy, unsigned long colormap, const char* spec, void* exact_def_return) { return 1; }
int X11Bridge::GetErrorText(void* dpy, int code, char* buffer, int length) { return 0; }
int X11Bridge::SetCloseDownMode(void* dpy, int close_mode) { return 1; }

int X11Bridge::StringListToTextProperty(char** list, int count, void* text_prop_return) {
    if (text_prop_return && count > 0 && list && list[0]) {
        DummyTextProperty* prop = (DummyTextProperty*)text_prop_return;
        size_t len = strlen(list[0]);
        prop->value = (unsigned char*)x_calloc(1, len + 1 + 16);
        if (prop->value) strncpy((char*)prop->value, list[0], len);
        prop->encoding = 31; prop->format = 8; prop->nitems = len;
        return 1;
    }
    return 0;
}

int X11Bridge::AutoRepeatOn(void* dpy) { return 1; }
int X11Bridge::VidModeQueryExtension(void* dpy, int* event_base_return, int* error_base_return) { return 1; }
int X11Bridge::VidModeGetAllModeLines(void* dpy, int screen, int* modecount_return, void*** modesinfo_return) {
    if (modecount_return) *modecount_return = 0;
    return 1;
}
int X11Bridge::VidModeSwitchToMode(void* dpy, int screen, void* modeline) { return 1; }
int X11Bridge::VidModeSetViewPort(void* dpy, int screen, int x, int y) { return 1; }
int X11Bridge::VidModeGetViewPort(void* dpy, int screen, int* x_return, int* y_return) { return 1; }
int X11Bridge::VidModeGetModeLine(void* dpy, int screen, int* dotclock_return, void* modeline) { return 0; }

// ---------------------------------------------------------
// New Allocators
// ---------------------------------------------------------
void* X11Bridge::XAllocWMHints() {
    return x_calloc(1, sizeof(DummyXWMHints));
}

void* X11Bridge::XAllocClassHint() {
    return x_calloc(1, sizeof(DummyXClassHint));
}

void* X11Bridge::XAllocSizeHints() {
    return x_calloc(1, sizeof(DummyXSizeHints));
}