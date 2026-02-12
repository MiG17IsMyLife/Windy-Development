#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#ifdef __linux__
#include "x11types.h"
#endif

// SDL Wrapper / Bridge
class SDLCalls
{
  public:
    static void Init();
    static void PollEvents();
    static void SwapBuffers();
    static void Quit();

    static void Start(int *argcp, char **argv);

    // Accessors
    static SDL_Window *GetWindow();
    static SDL_GLContext GetContext();
    static TTF_Font *GetFont();

#ifdef __linux__
    static Display *GetX11Display();
    static Window GetX11Window();
#endif

  private:
    static SDL_Window *m_window;
    static SDL_GLContext m_context;
    static TTF_Font *m_font;

#ifdef __linux__
    static Display *m_x11Display;
    static Window m_x11Window;
#endif
};