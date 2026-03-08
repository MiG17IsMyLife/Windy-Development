#include "SDL3/SDL_events.h"
#include <math.h>
#include "lindberghdevice.h"
#include "input.h"

void Input::sendShootingGameInput(SDL_Event *event)
{
 
    // SDL_PumpEvents();


    JvsBoard* jvs = LindberghDevice::Instance().GetJvsBoard();
    if (!jvs) {
        static bool warned = false;
        if (!warned) { printf("ProcessGLXEvents: JVS Board is NULL!"); warned = true; }
    }   
    if (jvs) {

        const bool *state = SDL_GetKeyboardState(NULL); 
      

        jvs->SetSwitch(PLAYER_1, BUTTON_SERVICE, state[SDL_SCANCODE_S]);
        
        jvs->SetSwitch(PLAYER_1, BUTTON_START, state[SDL_SCANCODE_1]);

        jvs->SetSwitch(SYSTEM, BUTTON_TEST, state[SDL_SCANCODE_T]);

        // jvs->SetSwitch(PLAYER_1, BUTTON_1, state[SDL_SCANCODE_Z]);
        // jvs->SetSwitch(PLAYER_1, BUTTON_2, state[SDL_SCANCODE_X]);
        
        static bool oldCoin = false;
        bool coin = state[SDL_SCANCODE_5];
        if (coin && !oldCoin) {
            jvs->IncrementCoin(PLAYER_1, 1);
        }
        oldCoin = coin;     
        
        float mx, my;
        uint32_t mouseMask = SDL_GetMouseState(&mx, &my);

        jvs->SetSwitch(PLAYER_1, BUTTON_1, (mouseMask & SDL_BUTTON_LMASK) != 0);
        jvs->SetSwitch(PLAYER_1, BUTTON_2, (mouseMask & SDL_BUTTON_RMASK) != 0);       


        if (getConfig()->width > 0 && getConfig()->height > 0) {
            int a1 = (int)((mx / (float)getConfig()->width) * 1023.0f);
            if (a1 < 0) a1 = 0; if (a1 > 1023) a1 = 1023;
            
            int a2 = (int)((my / (float)getConfig()->height) * 1023.0f);
            if (a2 < 0) a2 = 0; if (a2 > 1023) a2 = 1023;

            jvs->SetAnalogue(ANALOGUE_1, a1);
            jvs->SetAnalogue(ANALOGUE_2, a2);
        }

    }
}