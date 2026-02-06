#include "glutbridge.h"
#include "sdlcalls.h"
#include "x11bridge.h"
#include "../../core/log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../core/config.h"
#include "../../../include/LiberationMono-Regular.h"
#include <malloc.h>
#include <cstdlib>

// GLUT Constants (Minimal)
#define GLUT_RGBA 0x0000
#define GLUT_DOUBLE 0x0002
#define GLUT_DEPTH 0x0010
#define GLUT_STENCIL 0x0020

// GLUT Callbacks
static void (*g_glutDisplayFunc)(void) = nullptr;
static void (*g_glutIdleFunc)(void) = nullptr;
static bool g_redisplayNeeded = true;

// ==========================================================================
//   Helper Functions
// ==========================================================================

void convertSurfaceTo1Bit(SDL_Surface *surface, uint8_t *outBitmap, int pitch)
{
    SDL_LockSurface(surface);
    for (int y = 0; y < surface->h; ++y)
    {
        uint8_t byte = 0;
        int bit = 0;
        int sdlY = surface->h - 1 - y;

        Uint8 *row = (Uint8 *)surface->pixels + sdlY * surface->pitch;

        for (int x = 0; x < surface->w; ++x)
        {
            if (row[x])
                byte |= (1 << (7 - bit));

            bit++;
            if (bit == 8 || x == surface->w - 1)
            {
                outBitmap[y * pitch + x / 8] = byte;
                byte = 0;
                bit = 0;
            }
        }
    }
    SDL_UnlockSurface(surface);
}

void EnsureSDLInitialized()
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
        initialized = true;
    }
}

// ==========================================================================
//   GLUT Implementation
// ==========================================================================

void GLUTBridge::glutInit(int * /*argcp*/, char ** /*argv*/)
{
    log_info("glutInit");
    SDLCalls::Init();
}

void GLUTBridge::glutMainLoop()
{
    log_info(">>> glutMainLoop <<<");

    if (!SDL_GL_GetCurrentContext() && SDLCalls::GetWindow())
    {
        SDL_GL_MakeCurrent(SDLCalls::GetWindow(), SDLCalls::GetContext());
    }

    bool running = true;
    XEvent ev;
    Display *dpy = (Display *)0x1111; // Dummy

    while (running)
    {
        SDLCalls::PollEvents();
        while (X11Bridge::Pending(dpy))
        {
            X11Bridge::NextEvent(dpy, &ev);
        }

        if (g_glutIdleFunc)
            g_glutIdleFunc();

        if (g_redisplayNeeded && g_glutDisplayFunc)
        {
            g_glutDisplayFunc();
            g_redisplayNeeded = false;
        }

        SDL_Delay(1);
    }
}

void GLUTBridge::glutSwapBuffers()
{
    if (getConfig()->gameGroup == GROUP_OUTRUN || getConfig()->gameGroup == GROUP_OUTRUN_TEST)
        SDLCalls::PollEvents();

    if (SDLCalls::GetWindow())
    {
        SDLCalls::SwapBuffers();
    }
}

void GLUTBridge::glutInitDisplayMode(unsigned int mode)
{
    log_debug("glutInitDisplayMode(0x%X)", mode);
    // EnsureSDLInitialized();

    if (mode & GLUT_DOUBLE)
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    else
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

    if (mode & GLUT_DEPTH)
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    if (mode & GLUT_STENCIL)
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}

void GLUTBridge::glutInitWindowSize(int /*width*/, int /*height*/)
{
}

void GLUTBridge::glutInitWindowPosition(int /*x*/, int /*y*/)
{
}

int GLUTBridge::glutCreateWindow(const char *title)
{
    log_info("glutCreateWindow: %s", title);
    if (!SDLCalls::GetWindow())
        SDLCalls::Start(NULL, NULL);
    return 1;
}

int GLUTBridge::glutEnterGameMode()
{
    log_info("glutEnterGameMode");
    if (!SDLCalls::GetWindow())
        SDLCalls::Start(NULL, NULL);
    return 0;
}

void GLUTBridge::glutDisplayFunc(void (*func)(void))
{
    g_glutDisplayFunc = func;
}
void GLUTBridge::glutIdleFunc(void (*func)(void))
{
    g_glutIdleFunc = func;
}

void GLUTBridge::glutPostRedisplay()
{
    g_redisplayNeeded = true;
}

int GLUTBridge::glutGet(int state)
{
    if (state == 0x66)
        return getConfig()->width;
    if (state == 0x67)
        return getConfig()->height;

    return 0;
}

void GLUTBridge::glutSetCursor(int /*cursor*/)
{
}

void GLUTBridge::glutGameModeString(const char * /*string*/)
{
}

void GLUTBridge::glutJoystickFunc(void (* /*callback*/)(unsigned int, int, int, int), int /*pollInterval*/)
{
}

void GLUTBridge::glutKeyboardFunc(void (* /*callback*/)(unsigned char, int, int))
{
}

void GLUTBridge::glutKeyboardUpFunc(void (* /*callback*/)(unsigned char, int, int))
{
}

void GLUTBridge::glutMouseFunc(void (* /*callback*/)(int, int, int, int))
{
}

void GLUTBridge::glutMotionFunc(void (* /*callback*/)(int, int))
{
}

void GLUTBridge::glutSpecialFunc(void (* /*callback*/)(int, int, int))
{
}

void GLUTBridge::glutSpecialUpFunc(void (* /*callback*/)(int, int, int))
{
}

void GLUTBridge::glutPassiveMotionFunc(void (* /*callback*/)(int, int))
{
}

void GLUTBridge::glutEntryFunc(void (* /*callback*/)(int))
{
}

void GLUTBridge::glutLeaveGameMode()
{
}

void GLUTBridge::glutSolidTeapot(double /*size*/)
{
}

void GLUTBridge::glutWireTeapot(double /*size*/)
{
}

void GLUTBridge::glutSolidSphere(double /*radius*/, GLint /*slices*/, GLint /*stacks*/)
{
}

void GLUTBridge::glutWireSphere(double /*radius*/, GLint /*slices*/, GLint /*stacks*/)
{
}

void GLUTBridge::glutWireCone(double /*base*/, double /*height*/, GLint /*slices*/, GLint /*stacks*/)
{
}

void GLUTBridge::glutSolidCone(double /*base*/, double /*height*/, GLint /*slices*/, GLint /*stacks*/)
{
}

void GLUTBridge::glutWireCube(double /*dSize*/)
{
}

void GLUTBridge::glutSolidCube(double /*dSize*/)
{
}

void GLUTBridge::glutBitmapCharacter(void * /*fontID*/, int character)
{
    static int penX = 0;

    GLfloat glColor[4];
    glGetFloatv(GL_CURRENT_COLOR, glColor);

    SDL_Color sdlColor;
    sdlColor.r = (Uint8)(glColor[0] * 255.0f);
    sdlColor.g = (Uint8)(glColor[1] * 255.0f);
    sdlColor.b = (Uint8)(glColor[2] * 255.0f);
    sdlColor.a = (Uint8)(glColor[3] * 255.0f);

    GLint glPos[4];
    glGetIntegerv(GL_CURRENT_RASTER_POSITION, glPos);

    if (penX != glPos[0])
        penX = glPos[0];

    SDL_Surface *glyph = TTF_RenderGlyph_Solid(SDLCalls::GetFont(), character, sdlColor);
    if (!glyph)
    {
        printf("Failed to render glyph: %s\n", SDL_GetError());
        return;
    }

    int pitch = (glyph->w + 7) / 8;
    uint8_t *bitmap = (uint8_t *)malloc(pitch * glyph->h);
    if (!bitmap)
    {
        printf("Failed to allocate memory for bitmap\n");
        SDL_DestroySurface(glyph);
        return;
    }
    convertSurfaceTo1Bit(glyph, bitmap, pitch);

    GLint swbytes, lsbfirst, rowlen, skiprows, skippix, align;

    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swbytes);
    glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlen);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippix);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);

    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBitmap(glyph->w, glyph->h, 0, 0, (float)(glyph->w), 0.0, bitmap);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION, glPos);

    glPixelStorei(GL_UNPACK_SWAP_BYTES, swbytes);
    glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippix);
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);

    free(bitmap);
    SDL_DestroySurface(glyph);
}
int GLUTBridge::glutBitmapWidth(void * /*font*/, int /*character*/)
{
    return 8;
}
void GLUTBridge::glutMainLoopEvent()
{
    SDLCalls::PollEvents();
}
