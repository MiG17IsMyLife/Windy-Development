#include "sdlcalls.h"
#include "../../core/config.h"
#include "../../core/log.h"
#include "../../hardware/jvsboard.h"
#include "../../hardware/lindberghdevice.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../../include/LiberationMono-Regular.h"

// Static member initialization
TTF_Font* SDLCalls::m_font = nullptr;
SDL_Window* SDLCalls::m_window = nullptr;
SDL_GLContext SDLCalls::m_context = nullptr;
Display* SDLCalls::m_x11Display = nullptr;
Window SDLCalls::m_x11Window = 0;

void SDLCalls::Init() {
    static bool initialized = false;
    if (!initialized) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            log_error("SDL_Init failed: %s", SDL_GetError());
        } else {
            log_info("SDL3 initialized successfully");
        }

        if (!TTF_Init()) {
            log_error("SDL_ttf could not initialize! SDL_ttf Error: %s", SDL_GetError());
        }

        SDL_IOStream *rw = SDL_IOFromConstMem(LiberationMonoRegular_ttf, sizeof(LiberationMonoRegular_ttf));
        float fontSize = 16.0;
        m_font = TTF_OpenFontIO(rw, 1, fontSize);
        initialized = true;
    }
}

void SDLCalls::Quit() {
    log_info("SDLCalls::Quit() called");
    if (TTF_WasInit()) {
        if (m_font) {
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }
        TTF_Quit();
    }

    if (m_context) {
        SDL_GL_DestroyContext(m_context);
        m_context = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    exit(0);
}

void SDLCalls::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                Quit();
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                if (event.key.scancode == SDL_SCANCODE_T) {
                    LindberghDevice::Instance().SetSwitch(SYSTEM, JVSInput::BUTTON_TEST, event.type == SDL_EVENT_KEY_DOWN);
                }
                if (event.key.scancode == SDL_SCANCODE_S) {
                    LindberghDevice::Instance().SetSwitch(PLAYER_1, JVSInput::BUTTON_SERVICE, event.type == SDL_EVENT_KEY_DOWN);
                }
                if (event.key.scancode == SDL_SCANCODE_5) {
                    LindberghDevice::Instance().SetSwitch(PLAYER_1, JVSInput::COIN, event.type == SDL_EVENT_KEY_DOWN);
                }
                if (event.key.scancode == SDL_SCANCODE_1) {
                    LindberghDevice::Instance().SetSwitch(PLAYER_1, JVSInput::BUTTON_START, event.type == SDL_EVENT_KEY_DOWN);
                }
                break;
            }
            default:
                break;
        }
    }
}

void SDLCalls::SwapBuffers() {
     if (m_window)
        SDL_GL_SwapWindow(m_window);
}

void SDLCalls::Start(int *argcp, char **argv)
{
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

    uint32_t windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

    int width = getConfig()->width;
    int height = getConfig()->height;

    m_window = SDL_CreateWindow(getConfig()->gameTitle, width, height, windowFlags);

    if (!m_window)
    {
        SDL_Quit();
        log_error("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    m_x11Display = (Display *)SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    m_x11Window = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (!m_x11Display || !m_x11Window)
    {
        log_error("This program is not running on X11 or failed to get window/display.\n");
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context)
    {
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        log_error("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_SetWindowMinimumSize(m_window, width, height);

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
SDL_Window* SDLCalls::GetWindow() { return m_window; }
SDL_GLContext SDLCalls::GetContext() { return m_context; }
TTF_Font* SDLCalls::GetFont() { return m_font; }
Display* SDLCalls::GetX11Display() { return m_x11Display; }
Window SDLCalls::GetX11Window() { return m_x11Window; }
