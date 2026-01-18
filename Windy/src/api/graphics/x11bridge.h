#ifndef __X11_BRIDGE_H
#define __X11_BRIDGE_H

#include <windows.h>

class X11Bridge {
private:
    static HWND m_hWnd;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    static int InitThreads();
    static void* OpenDisplay(const char* name);

    static unsigned long XCreateWindow(void* dpy, unsigned long parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_type, void* visual, unsigned long valuemask, void* attributes);

    static void MapWindow(void* dpy, unsigned long win);
    static int Pending(void* dpy);
    static void NextEvent(void* dpy, void* event_return);
    static int Sync(void* dpy, int discard);
    static int Flush(void* dpy);
    static void Free(void* data);
    static void SetInputFocus(void* dpy, unsigned long win, int revert, unsigned long time);
    static void SetErrorHandler(void* handler);
    static unsigned long InternAtom(void* dpy, const char* name, int only_if_exists);
    static void StoreName(void* dpy, unsigned long win, const char* name);
    static int LookupString(void* event, char* buffer, int n, void* k, void* s);
    static void DefineCursor(void* d, unsigned long w, unsigned long c);
    static void WarpPointer(void* d, unsigned long s, unsigned long de, int sx, int sy, unsigned int sw, unsigned int sh, int dx, int dy);
    static void GrabKeyboard(void* d, unsigned long w, int o, int p, int k, unsigned long t);
    static void GrabPointer(void* d, unsigned long w, int o, unsigned int m, int p, int k, unsigned long c, unsigned long cu, unsigned long t);
    static void UngrabKeyboard(void* d, unsigned long t);
    static void UngrabPointer(void* d, unsigned long t);
    static int CloseDisplay(void* dpy);

    static int SetWMProtocols(void* dpy, unsigned long win, unsigned long* protocols, int count);
    static int GetWindowAttributes(void* dpy, unsigned long win, void* attr);
    static int SelectInput(void* dpy, unsigned long win, long event_mask);
    static unsigned long CreateColormap(void* dpy, unsigned long win, void* visual, int alloc);
    static void SetWMName(void* dpy, unsigned long win, void* text_prop);
    static int SetWMHints(void* dpy, unsigned long win, void* hints);
    static int SetWMProperties(void* dpy, unsigned long win, void* window_name, void* icon_name, char** argv, int argc, void* normal_hints, void* wm_hints, void* class_hints);
    static int ChangeProperty(void* dpy, unsigned long win, unsigned long property, unsigned long type, int format, int mode, const unsigned char* data, int nelements);
    static int GetGeometry(void* dpy, unsigned long drawable, unsigned long* root_return, int* x_return, int* y_return, unsigned int* width_return, unsigned int* height_return, unsigned int* border_width_return, unsigned int* depth_return);
    static int TranslateCoordinates(void* dpy, unsigned long src_w, unsigned long dest_w, int src_x, int src_y, int* dest_x_return, int* dest_y_return, unsigned long* child_return);
    static int QueryPointer(void* dpy, unsigned long win, unsigned long* root_return, unsigned long* child_return, int* root_x_return, int* root_y_return, int* win_x_return, int* win_y_return, unsigned int* mask_return);

    static int MoveWindow(void* dpy, unsigned long win, int x, int y);
    static int DestroyWindow(void* dpy, unsigned long win);
    static void LockDisplay(void* dpy);
    static void UnlockDisplay(void* dpy);

    static int FreePixmap(void* dpy, unsigned long pixmap);
    static int FreeCursor(void* dpy, unsigned long cursor);
    static unsigned long CreatePixmapCursor(void* dpy, unsigned long source, unsigned long mask, void* foreground_color, void* background_color, unsigned int x, unsigned int y);
    static unsigned long CreatePixmapFromBitmapData(void* dpy, unsigned long drawable, char* data, unsigned int width, unsigned int height, unsigned long fg, unsigned long bg, unsigned int depth);

    static int ParseColor(void* dpy, unsigned long colormap, const char* spec, void* exact_def_return);
    static int GetErrorText(void* dpy, int code, char* buffer, int length);
    static int SetCloseDownMode(void* dpy, int close_mode);
    static int StringListToTextProperty(char** list, int count, void* text_prop_return);
    static int AutoRepeatOn(void* dpy);

    static int VidModeQueryExtension(void* dpy, int* event_base_return, int* error_base_return);
    static int VidModeGetAllModeLines(void* dpy, int screen, int* modecount_return, void*** modesinfo_return);
    static int VidModeSwitchToMode(void* dpy, int screen, void* modeline);
    static int VidModeSetViewPort(void* dpy, int screen, int x, int y);
    static int VidModeGetViewPort(void* dpy, int screen, int* x_return, int* y_return);
    static int VidModeGetModeLine(void* dpy, int screen, int* dotclock_return, void* modeline);

    static HWND GetHwnd() { return m_hWnd; }
};

#endif