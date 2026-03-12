/**
 * @file input.h
 * @brief Input handler - delegates to SdlInput system
 */
#pragma once

#include "SDL3/SDL_events.h"

class Input {
public:
    /// Initialize the input system (call once after SDL and JVS are ready)
    static void Init(const char* controlsPath = "controls.ini");

    /// Process an SDL event through the input system
    static void ProcessEvent(const SDL_Event* event);

    /// Called once per frame to flush changed actions to JVS
    static void UpdatePerFrame();

    /// Legacy compatibility wrapper
    static void sendShootingGameInput(SDL_Event *event);
};