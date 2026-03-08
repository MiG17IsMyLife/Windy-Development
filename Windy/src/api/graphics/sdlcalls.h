#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "x11types.h"

// SDL Wrapper / Bridge
class SDLCalls
{
  public:
    static void Init();
    static void PollEvents();
    static void SwapBuffers();
    static void Quit();

    static void Start(int *argcp, char **argv);
    static void CreateSDLWindow(bool hidden = false);

    // Accessors
    static SDL_Window *GetWindow();
    static SDL_GLContext GetContext();
    static TTF_Font *GetFont();
    static int MakeCurrent(SDL_Window *window, SDL_GLContext context);

  private:
    static SDL_Window *m_window;
    static SDL_GLContext m_context;
    static TTF_Font *m_font;
    static Display *m_x11Display;
    static Window m_x11Window;
};
