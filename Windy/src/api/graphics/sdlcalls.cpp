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
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
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

        // Initialize input system now that SDL is fully up
        Input::Init("controls.ini");

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
            default:
                // Forward all events to the input system
                Input::ProcessEvent(&event);
                break;
        }
    }
    // Flush changed actions to JVS once per frame
    Input::UpdatePerFrame();
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
        return;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    const int width = getConfig()->width;
    const int height = getConfig()->height;

    uint32_t windowFlags = SDL_WINDOW_OPENGL;
    if (hidden)
        windowFlags |= SDL_WINDOW_HIDDEN;

    m_window = SDL_CreateWindow(getConfig()->gameTitle, width, height, windowFlags);
    if (!m_window)
    {
        log_error("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
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

    if (getConfig()->fullscreen)
    {
        SDL_DisplayMode mode = {};
        mode.w = width;
        mode.h = height;
        mode.pixel_density = 1.0f;
        mode.refresh_rate = 0.0f;
        mode.format = SDL_PIXELFORMAT_UNKNOWN;

        if (!SDL_SetWindowFullscreenMode(m_window, &mode))
            log_warn("SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError());

        if (!SDL_SetWindowFullscreen(m_window, true))
            log_warn("SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
    }

    if (!SDL_GL_SetSwapInterval(getConfig()->vsync ? 1 : 0))
        log_warn("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());

    int drawableW = 0;
    int drawableH = 0;
    SDL_GetWindowSizeInPixels(m_window, &drawableW, &drawableH);

    log_info("SDL window created: config=%dx%d fullscreen=%s vsync=%s drawable=%dx%d", width, height,
             getConfig()->fullscreen ? "true" : "false", getConfig()->vsync ? "true" : "false", drawableW, drawableH);

    SDL_ShowWindow(m_window);

    Uint64 startTime = SDL_GetTicks();
    int running = 1;
    while (running)
    {
        if (SDL_GetTicks() - startTime >= 1000)
            running = 0;

        glViewport(0, 0, width, height);
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

