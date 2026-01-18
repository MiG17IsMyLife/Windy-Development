#include "X11Bridge.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#pragma warning(disable:4996)

HWND X11Bridge::m_hWnd = NULL;

// TODO: Windows size should be configrable from game
static const int DEFAULT_WINDOW_WIDTH = 1360;
static const int DEFAULT_WINDOW_HEIGHT = 768;

// ---------------------------------------------------------
// Memory Helper (Aligned to match symbolresolver.cpp)
// ---------------------------------------------------------
static void* x_malloc(size_t size) {
    return _aligned_malloc(size, 16);
}

static void* x_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void* p = _aligned_malloc(total, 16);
    if (p) memset(p, 0, total);
    return p;
}

static void x_free(void* p) {
    if (p) _aligned_free(p);
}

// ---------------------------------------------------------
// Xlib Internal Structures (32bit Linux Layout)
// ---------------------------------------------------------

struct DummyXEvent {
    int type;
    unsigned long serial;
    int send_event;
    unsigned long display;
    unsigned long window;
    int x, y, width, height;
    int count;
    char padding[128];
};

struct DummyVisual {
    void* ext_data;
    unsigned long visualid;
    int class_type;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb;
    int map_entries;
};

struct DummyScreen {
    void* ext_data;
    unsigned long display;
    unsigned long root;
    int width, height;
    int mwidth, mheight;
    int ndepths;
    void* depths;
    int root_depth;
    void* root_visual;
    void* default_gc;
    unsigned long cmap;
    unsigned long white_pixel;
    unsigned long black_pixel;
    int max_maps, min_maps;
    int backing_store;
    int save_unders;
    long root_input_mask;
};

struct DummyDisplay {
    void* ext_data;
    void* free_funcs;
    int fd;
    int conn_checker;
    int proto_major_version;
    int proto_minor_version;
    char* vendor;
    unsigned long resource_base;
    unsigned long resource_mask;
    unsigned long resource_id;
    int resource_shift;
    void* resource_alloc;
    int byte_order;
    int bitmap_unit;
    int bitmap_pad;
    int bitmap_bit_order;
    int nformats;
    void* pixmap_format;
    int vnumber;
    int release;
    void* head;
    void* tail;
    int qlen;
    unsigned long last_request_read;
    unsigned long request;
    char* last_req;
    char* buffer;
    char* bufptr;
    char* bufmax;
    unsigned max_request_size;
    void* db;
    void* synchandler;
    char* display_name;
    int default_screen;
    int nscreens;
    DummyScreen* screens;
    unsigned long motion_buffer;
    unsigned long flags;
    int min_keycode;
    int max_keycode;
    void* keysyms;
    void* modifiermap;
    int keysyms_per_keycode;
    char* xdefaults;
    char* scratch_buffer;
    unsigned long scratch_length;
    int ext_number;
    void* ext_procs;
    void* event_vec[128];
    void* wire_vec[128];
    unsigned long lock_meaning;
    void* lock;
    void* internal_async;
    int internal_async_len;
    void* internal_await;
    int internal_await_len;
    void* savedsynchandler;
    unsigned long resource_max;
    int xcmisc_opcode;
    void* xim;
    void* bigreq;
    void* support;
    void* context_db;
    void* error_vec;
    void* cms;
    void* im_filters;
    void* qfree;
    unsigned long next_event_serial_num;
    void* flushes;
    void* im_fd_info;
    int im_fd_length;
    void* conn_watchers;
    int watcher_count;
    void* filedes;
    void* savedsynchander;
    unsigned long client_leader;
    unsigned long window_group;
    unsigned long fingerprint;
};

struct DummyTextProperty {
    unsigned char* value;
    unsigned long encoding;
    int format;
    unsigned long nitems;
};

// ---------------------------------------------------------

LRESULT CALLBACK X11Bridge::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int X11Bridge::InitThreads() {
    std::cout << "[X11] InitThreads called" << std::endl;
    return 1;
}

void* X11Bridge::OpenDisplay(const char* name) {
    std::cout << "[X11] OpenDisplay called: " << (name ? name : "NULL") << std::endl;

    DummyDisplay* dpy = (DummyDisplay*)x_calloc(1, sizeof(DummyDisplay) + 4096);
    DummyScreen* scr = (DummyScreen*)x_calloc(1, sizeof(DummyScreen));
    DummyVisual* vis = (DummyVisual*)x_calloc(1, sizeof(DummyVisual));

    if (dpy && scr && vis) {
        vis->visualid = 0x21;
        vis->class_type = 4;
        vis->bits_per_rgb = 8;
        vis->red_mask = 0xFF0000;
        vis->green_mask = 0x00FF00;
        vis->blue_mask = 0x0000FF;
        vis->map_entries = 256;

        scr->display = (unsigned long)dpy;
        scr->root = (unsigned long)GetDesktopWindow();

        // Currently focusing to Initial D4/5 resolution
        scr->width = DEFAULT_WINDOW_WIDTH;
        scr->height = DEFAULT_WINDOW_HEIGHT;

        scr->mwidth = 320;
        scr->mheight = 240;
        scr->root_depth = 24;
        scr->root_visual = vis;
        scr->white_pixel = 0xFFFFFFFF;
        scr->black_pixel = 0x00000000;
        scr->max_maps = 1;
        scr->min_maps = 1;

        // Display initializing
        dpy->default_screen = 0;
        dpy->nscreens = 1;
        dpy->screens = scr;
        dpy->byte_order = 0; // LSBFirst
        dpy->bitmap_unit = 32;
        dpy->bitmap_pad = 32;
        dpy->proto_major_version = 11;

        dpy->buffer = (char*)x_calloc(1, 4096);
        dpy->bufptr = dpy->buffer;
        dpy->bufmax = dpy->buffer + 4096;

        std::cout << "[X11] Allocated initialized Display at " << dpy << ", Screens at " << scr << std::endl;
        return dpy;
    }
    return NULL;
}

int X11Bridge::CloseDisplay(void* dpy) {
    std::cout << "[X11] CloseDisplay called" << std::endl;
    if (dpy) {
        DummyDisplay* d = (DummyDisplay*)dpy;
        if (d->buffer) x_free(d->buffer);
        if (d->screens) {
            DummyScreen* s = d->screens;
            if (s->root_visual) x_free(s->root_visual);
            x_free(s);
        }
        x_free(d);
    }
    return 0;
}

unsigned long X11Bridge::XCreateWindow(void* dpy, unsigned long parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_type, void* visual, unsigned long valuemask, void* attributes) {
    std::cout << "[X11] XCreateWindow called (Requested: " << width << "x" << height << ")" << std::endl;

    // TODO: use requested size from game if needed, but usually we enforce the Lindbergh standard resolution
    width = DEFAULT_WINDOW_WIDTH;
    height = DEFAULT_WINDOW_HEIGHT;
    std::cout << "[X11] -> Forcing Window Size to: " << width << "x" << height << std::endl;

    HINSTANCE hInst = GetModuleHandleA(NULL);
    const char* className = "WindyX11Window";
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_OWNDC, WindowProc, 0, 0, hInst, NULL, NULL, NULL, NULL, className, NULL };
    RegisterClassExA(&wc);

    m_hWnd = CreateWindowExA(0, className, "Windy - Sega Lindbergh",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInst, NULL);

    if (m_hWnd) {
        std::cout << "[X11] Window created successfully. HWND: " << std::hex << m_hWnd << std::endl;
    }
    else {
        std::cerr << "[X11] FAILED to create window! Error: " << GetLastError() << std::endl;
    }

    return (unsigned long)m_hWnd;
}

void X11Bridge::MapWindow(void* dpy, unsigned long win) {
    std::cout << "[X11] MapWindow called for HWND: " << std::hex << win << std::endl;
    ShowWindow((HWND)win, SW_SHOW);
    UpdateWindow((HWND)win);
}

int X11Bridge::Pending(void* dpy) { return 1; }

void X11Bridge::NextEvent(void* dpy, void* event_return) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DummyXEvent* ev = (DummyXEvent*)event_return;
    memset(ev, 0, sizeof(DummyXEvent));

    static bool mapped = false;

    if (!mapped) {
        ev->type = 19; // MapNotify
        ev->serial = 1;
        ev->send_event = 0;
        ev->display = (unsigned long)dpy;
        ev->window = (unsigned long)m_hWnd;
        std::cout << "[X11] NextEvent returning MapNotify" << std::endl;
        mapped = true;
        return;
    }

    Sleep(1);
    ev->type = 12;
    ev->serial = 2;
    ev->send_event = 0;
    ev->display = (unsigned long)dpy;
    ev->window = (unsigned long)m_hWnd;

    RECT rect;
    if (GetWindowRect(m_hWnd, &rect)) {
        ev->width = rect.right - rect.left;
        ev->height = rect.bottom - rect.top;
    }
    else {
        ev->width = DEFAULT_WINDOW_WIDTH; ev->height = DEFAULT_WINDOW_HEIGHT;
    }
}

int X11Bridge::Sync(void* dpy, int discard) { return 0; }
int X11Bridge::Flush(void* dpy) { return 0; }
void X11Bridge::Free(void* data) { if (data) x_free(data); }
void X11Bridge::SetInputFocus(void* dpy, unsigned long win, int revert, unsigned long time) { SetFocus((HWND)win); }
void X11Bridge::SetErrorHandler(void* handler) { std::cout << "[X11] SetErrorHandler called" << std::endl; }

unsigned long X11Bridge::InternAtom(void* dpy, const char* name, int only_if_exists) {
    std::cout << "[X11] InternAtom called: " << (name ? name : "NULL") << std::endl;
    unsigned long hash = 0;
    if (name) { for (const char* p = name; *p; p++) hash = hash * 31 + *p; }
    return hash ? hash : 0x123;
}

void X11Bridge::StoreName(void* dpy, unsigned long win, const char* name) {
    std::cout << "[X11] StoreName called: " << name << std::endl;
    SetWindowTextA((HWND)win, name);
}

int X11Bridge::LookupString(void* event, char* buffer, int n, void* k, void* s) { return 0; }
void X11Bridge::DefineCursor(void* d, unsigned long w, unsigned long c) { if (c == 0) SetCursor(NULL); else SetCursor(LoadCursor(NULL, IDC_ARROW)); }
void X11Bridge::WarpPointer(void* d, unsigned long s, unsigned long de, int sx, int sy, unsigned int sw, unsigned int sh, int dx, int dy) {
    POINT pt = { dx, dy }; if (de) ClientToScreen((HWND)de, &pt); SetCursorPos(pt.x, pt.y);
}
void X11Bridge::GrabKeyboard(void* d, unsigned long w, int o, int p, int k, unsigned long t) { SetFocus((HWND)w); }
void X11Bridge::GrabPointer(void* d, unsigned long w, int o, unsigned int m, int p, int k, unsigned long c, unsigned long cu, unsigned long t) { SetCapture((HWND)w); }
void X11Bridge::UngrabKeyboard(void* d, unsigned long t) {}
void X11Bridge::UngrabPointer(void* d, unsigned long t) { ReleaseCapture(); }

// ========================================
// WM & Properties
// ========================================

int X11Bridge::SetWMProtocols(void* dpy, unsigned long win, unsigned long* protocols, int count) {
    std::cout << "[X11] SetWMProtocols called (Count: " << count << ")" << std::endl;
    return 1;
}

int X11Bridge::GetWindowAttributes(void* dpy, unsigned long win, void* attr) {
    RECT rect; return GetWindowRect((HWND)win, &rect) ? 1 : 0;
}

int X11Bridge::SelectInput(void* dpy, unsigned long win, long event_mask) {
    std::cout << "[X11] SelectInput called (Mask: " << std::hex << event_mask << ")" << std::endl;
    return 1;
}

unsigned long X11Bridge::CreateColormap(void* dpy, unsigned long win, void* visual, int alloc) {
    std::cout << "[X11] CreateColormap called" << std::endl;
    static unsigned long dummy_colormap = 0xAABBCC;
    return dummy_colormap;
}

void X11Bridge::SetWMName(void* dpy, unsigned long win, void* text_prop) { std::cout << "[X11] SetWMName called" << std::endl; }
int X11Bridge::SetWMHints(void* dpy, unsigned long win, void* hints) { std::cout << "[X11] SetWMHints called" << std::endl; return 1; }

int X11Bridge::SetWMProperties(void* dpy, unsigned long win, void* window_name, void* icon_name, char** argv, int argc, void* normal_hints, void* wm_hints, void* class_hints) {
    std::cout << "[X11] SetWMProperties called" << std::endl;
    return 1;
}

int X11Bridge::ChangeProperty(void* dpy, unsigned long win, unsigned long property, unsigned long type, int format, int mode, const unsigned char* data, int nelements) {
    std::cout << "[X11] ChangeProperty called" << std::endl;
    return 1;
}

int X11Bridge::GetGeometry(void* dpy, unsigned long drawable, unsigned long* root_return, int* x_return, int* y_return, unsigned int* width_return, unsigned int* height_return, unsigned int* border_width_return, unsigned int* depth_return) {
    std::cout << "[X11] GetGeometry called" << std::endl;
    RECT rect; if (!GetWindowRect((HWND)drawable, &rect)) return 0;
    if (x_return) *x_return = rect.left;
    if (y_return) *y_return = rect.top;
    if (width_return) *width_return = rect.right - rect.left;
    if (height_return) *height_return = rect.bottom - rect.top;
    if (border_width_return) *border_width_return = 0;
    if (depth_return) *depth_return = 24;
    if (root_return) *root_return = (unsigned long)GetDesktopWindow();
    return 1;
}

int X11Bridge::TranslateCoordinates(void* dpy, unsigned long src_w, unsigned long dest_w, int src_x, int src_y, int* dest_x_return, int* dest_y_return, unsigned long* child_return) {
    std::cout << "[X11] TranslateCoordinates called" << std::endl;
    POINT pt = { src_x, src_y };
    ClientToScreen((HWND)src_w, &pt); ScreenToClient((HWND)dest_w, &pt);
    if (dest_x_return) *dest_x_return = pt.x;
    if (dest_y_return) *dest_y_return = pt.y;
    if (child_return) *child_return = 0;
    return 1;
}

int X11Bridge::QueryPointer(void* dpy, unsigned long win, unsigned long* root_return, unsigned long* child_return, int* root_x_return, int* root_y_return, int* win_x_return, int* win_y_return, unsigned int* mask_return) {
    POINT pt; GetCursorPos(&pt);
    if (root_x_return) *root_x_return = pt.x; if (root_y_return) *root_y_return = pt.y;
    ScreenToClient((HWND)win, &pt);
    if (win_x_return) *win_x_return = pt.x; if (win_y_return) *win_y_return = pt.y;
    if (root_return) *root_return = (unsigned long)GetDesktopWindow();
    if (child_return) *child_return = 0;
    if (mask_return) *mask_return = 0;
    return 1;
}

int X11Bridge::MoveWindow(void* dpy, unsigned long win, int x, int y) {
    std::cout << "[X11] MoveWindow called" << std::endl;
    RECT rect; if (!GetWindowRect((HWND)win, &rect)) return 0;
    return ::MoveWindow((HWND)win, x, y, rect.right - rect.left, rect.bottom - rect.top, TRUE) ? 1 : 0;
}
int X11Bridge::DestroyWindow(void* dpy, unsigned long win) {
    std::cout << "[X11] DestroyWindow called" << std::endl;
    return ::DestroyWindow((HWND)win) ? 1 : 0;
}
void X11Bridge::LockDisplay(void* dpy) {}
void X11Bridge::UnlockDisplay(void* dpy) {}

int X11Bridge::FreePixmap(void* dpy, unsigned long pixmap) { return 1; }
int X11Bridge::FreeCursor(void* dpy, unsigned long cursor) { return 1; }
unsigned long X11Bridge::CreatePixmapCursor(void* dpy, unsigned long source, unsigned long mask, void* foreground_color, void* background_color, unsigned int x, unsigned int y) { return (unsigned long)LoadCursor(NULL, IDC_ARROW); }
unsigned long X11Bridge::CreatePixmapFromBitmapData(void* dpy, unsigned long drawable, char* data, unsigned int width, unsigned int height, unsigned long fg, unsigned long bg, unsigned int depth) {
    HDC hdc = GetDC((HWND)drawable); HBITMAP hBitmap = CreateBitmap(width, height, 1, depth, data); ReleaseDC((HWND)drawable, hdc); return (unsigned long)hBitmap;
}

int X11Bridge::ParseColor(void* dpy, unsigned long colormap, const char* spec, void* exact_def_return) { return 1; }
int X11Bridge::GetErrorText(void* dpy, int code, char* buffer, int length) { return 0; }
int X11Bridge::SetCloseDownMode(void* dpy, int close_mode) { return 1; }

int X11Bridge::StringListToTextProperty(char** list, int count, void* text_prop_return) {
    std::cout << "[X11] StringListToTextProperty called" << std::endl;
    if (text_prop_return && count > 0 && list && list[0]) {
        DummyTextProperty* prop = (DummyTextProperty*)text_prop_return;
        size_t len = strlen(list[0]);
        prop->value = (unsigned char*)x_calloc(1, len + 1 + 16);
        if (prop->value) {
            strcpy_s((char*)prop->value, len + 1, list[0]);
        }
        prop->encoding = 31; // XA_STRING
        prop->format = 8;
        prop->nitems = len;
        return 1;
    }
    return 0;
}

int X11Bridge::AutoRepeatOn(void* dpy) { return 1; }

int X11Bridge::VidModeQueryExtension(void* dpy, int* event_base_return, int* error_base_return) {
    std::cout << "[X11] VidModeQueryExtension called" << std::endl;
    if (event_base_return) *event_base_return = 0;
    if (error_base_return) *error_base_return = 0;
    return 1;
}
int X11Bridge::VidModeGetAllModeLines(void* dpy, int screen, int* modecount_return, void*** modesinfo_return) {
    if (modecount_return) *modecount_return = 0;
    if (modesinfo_return) *modesinfo_return = NULL;
    return 1;
}
int X11Bridge::VidModeSwitchToMode(void* dpy, int screen, void* modeline) { return 1; }
int X11Bridge::VidModeSetViewPort(void* dpy, int screen, int x, int y) { return 1; }
int X11Bridge::VidModeGetViewPort(void* dpy, int screen, int* x_return, int* y_return) { if (x_return) *x_return = 0; if (y_return) *y_return = 0; return 1; }
int X11Bridge::VidModeGetModeLine(void* dpy, int screen, int* dotclock_return, void* modeline) { return 0; }