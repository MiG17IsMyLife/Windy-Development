/**
 * @file input.cpp
 * @brief Input handler - delegates to SdlInput system
 */
#include "input.h"
#include "sdlinput.h"

void Input::Init(const char* controlsPath)
{
    SdlInput::Instance().Init(controlsPath);
}

void Input::ProcessEvent(const SDL_Event* event)
{
    SdlInput::Instance().ProcessEvent(event);
}

void Input::UpdatePerFrame()
{
    SdlInput::Instance().UpdateCombinedAxes();
    SdlInput::Instance().ProcessChangedActions();

    // HOD4 gun shake
    GameGroup grp = getConfig()->gameGroup;
    if (grp == GROUP_HOD4 || grp == GROUP_HOD4_TEST)
        SdlInput::Instance().UpdateGunShake();
}

// Legacy compatibility wrapper
void Input::sendShootingGameInput(SDL_Event *event)
{
    ProcessEvent(event);
}