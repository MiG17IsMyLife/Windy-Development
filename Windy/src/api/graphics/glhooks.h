#pragma once

class GLHooks
{
  public:
    static void *GetProcAddress(const char *procName);
};
