#include "sdlcalls.h"
#include "../../core/config.h"
#include "../../core/log.h"
#include "../../hardware/jvsboard.h"
#include "../../hardware/input.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../../include/LiberationMono-Regular.h"

// Static member initialization
TTF_Font *SDLCalls::m_font = nullptr;
SDL_Window *SDLCalls::m_window = nullptr;
SDL_GLContext SDLCalls::m_context = nullptr;
Display *SDLCalls::m_x11Display = nullptr;
Window SDLCalls::m_x11Window = 0;

void SDLCalls::Init()
{
    static bool initialized = false;
    if (!initialized)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            log_error("SDL_Init failed: %s", SDL_GetError());
        }
        else
        {
            log_info("SDL3 initialized successfully");
        }

        if (!TTF_Init())
        {
            log_error("SDL_ttf could not initialize! SDL_ttf Error: %s", SDL_GetError());
        }

        SDL_IOStream *rw = SDL_IOFromConstMem(LiberationMonoRegular_ttf, sizeof(LiberationMonoRegular_ttf));
        float fontSize = 16.0;
        m_font = TTF_OpenFontIO(rw, 1, fontSize);
        initialized = true;
    }
}

void SDLCalls::Quit()
{
    log_info("SDLCalls::Quit() called");
    if (TTF_WasInit())
    {
        if (m_font)
        {
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }
        TTF_Quit();
    }

    if (m_context)
    {
        SDL_GL_DestroyContext(m_context);
        m_context = nullptr;
    }

    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    exit(0);
}

void SDLCalls::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                Quit();
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_MOTION:
            {
                Input::sendShootingGameInput(&event);
                break;
            }
            default:
                break;
        }
    }
}

void SDLCalls::SwapBuffers()
{
    if (m_window)
        SDL_GL_SwapWindow(m_window);
}

void SDLCalls::Start(int *argcp, char **argv)
{
    // Moved argument parsing out or ignored for now
    CreateSDLWindow();
}

void SDLCalls::CreateSDLWindow(bool hidden)
{
    if (m_window)
        return; // Already created

    int numDisplays;
    SDL_DisplayID *sdlDisplayId = SDL_GetDisplays(&numDisplays);
    if (numDisplays > 1)
        log_warn("More than 1 display detected, will use the first one.\n");

    const SDL_DisplayMode *displayMode = SDL_GetCurrentDisplayMode(sdlDisplayId[0]);
    SDL_free(sdlDisplayId);
    if (displayMode == NULL)
    {
        log_error("SDL_GetCurrentDisplayMode Error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(EXIT_FAILURE);
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // Double buffering
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);  // 24-bit depth buffer
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);   // Set the alpha size to 8 bits
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    uint32_t windowFlags = SDL_WINDOW_OPENGL;
    if (hidden)
        windowFlags |= SDL_WINDOW_HIDDEN;

    int width = getConfig()->width;
    int height = getConfig()->height;

    printf("Creating window with width %d and height %d\n", width, height);

    m_window = SDL_CreateWindow(getConfig()->gameTitle, width, height, windowFlags);

    if (!m_window)
    {
        SDL_Quit();
        log_error("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context)
    {
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        log_error("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_GL_MakeCurrent(m_window, m_context);

    if (SDL_GL_SetSwapInterval(1) == false)
        log_warn("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
    // SDL_SetWindowMinimumSize(m_window, width, height);

    SDL_ShowWindow(m_window);

    Uint64 startTime = SDL_GetTicks();
    int running = 1;

    // We clear the window background
    while (running)
    {
        if (SDL_GetTicks() - startTime >= 1000)
            running = 0;
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(m_window);
    }
}

// Accessors
SDL_Window *SDLCalls::GetWindow()
{
    return m_window;
}
SDL_GLContext SDLCalls::GetContext()
{
    return m_context;
}
TTF_Font *SDLCalls::GetFont()
{
    return m_font;
}

int SDLCalls::MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    return SDL_GL_MakeCurrent(window, context);
}

