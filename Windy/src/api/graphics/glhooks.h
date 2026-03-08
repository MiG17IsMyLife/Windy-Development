#pragma once

#include "SDL3/SDL_opengl.h"
class GLHooks
{
  public:
    static void *GetProcAddress(const char *procName);
};
