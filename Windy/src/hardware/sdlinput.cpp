#define _CRT_SECURE_NO_WARNINGS

#include "sdlinput.h"
#include "lindberghdevice.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../core/iniparser.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdio>

// ============================================================
// Singleton
// ============================================================
SdlInput& SdlInput::Instance()
{
    static SdlInput instance;
    return instance;
}

SdlInput::SdlInput()
{
    memset(m_actionStates, 0, sizeof(m_actionStates));
    memset(m_jvsMap, 0, sizeof(m_jvsMap));
    memset(m_actionProps, 0, sizeof(m_actionProps));
    memset(m_changedActions, 0, sizeof(m_changedActions));
    memset(m_keyBindings, 0, sizeof(m_keyBindings));
    memset(m_mouseButtonBindings, 0, sizeof(m_mouseButtonBindings));
    memset(m_mouseAxisBindings, 0, sizeof(m_mouseAxisBindings));
    memset(m_joyAxisBindings, 0, sizeof(m_joyAxisBindings));
    memset(m_joyButtonBindings, 0, sizeof(m_joyButtonBindings));
    memset(m_controllerAxisBindings, 0, sizeof(m_controllerAxisBindings));
    memset(m_controllerButtonBindings, 0, sizeof(m_controllerButtonBindings));
    memset(m_joyHatBindings, 0, sizeof(m_joyHatBindings));
    memset(&m_controllers, 0, sizeof(m_controllers));
}

SdlInput::~SdlInput()
{
    Shutdown();
}

// ============================================================
// Init
// ============================================================
int SdlInput::Init(const char* controlsPath)
{
    if (m_initialized)
        return 0;

    // SDL_INIT_GAMEPAD is already handled by SDLCalls::Init()

    WindyConfig* cfg = getConfig();
    GameType gameType = cfg->gameType;
    GameID   gameId   = cfg->gameId;
    GameGroup grp     = cfg->gameGroup;

    JvsBoard* jvs = LindberghDevice::Instance().GetJvsBoard();
    if (jvs)
    {
        m_jvsAnalogueMaxValue = (1 << jvs->GetCapabilities().analogueInBits) - 1;
        if (m_jvsAnalogueMaxValue <= 0)
            m_jvsAnalogueMaxValue = 1023;
        m_jvsAnalogueCenterValue = m_jvsAnalogueMaxValue / 2;
    }

    // Set initial analog values for specific games
    if (jvs)
    {
        if (grp == GROUP_HOD4 || grp == GROUP_HOD4_TEST)
        {
            jvs->SetAnalogue(ANALOGUE_5, m_jvsAnalogueCenterValue);
            jvs->SetAnalogue(ANALOGUE_6, m_jvsAnalogueCenterValue);
            jvs->SetAnalogue(ANALOGUE_7, m_jvsAnalogueCenterValue);
            jvs->SetAnalogue(ANALOGUE_8, m_jvsAnalogueCenterValue);
        }
        else if (gameId == HARLEY_DAVIDSON)
        {
            jvs->SetAnalogue(ANALOGUE_2, m_jvsAnalogueCenterValue);
        }
        else if (gameType == DRIVING)
        {
            jvs->SetAnalogue(ANALOGUE_1, m_jvsAnalogueCenterValue);
            jvs->SetAnalogue(ANALOGUE_5, m_jvsAnalogueCenterValue);
        }
    }

    // Initialize mappings and properties
    InitJvsMappings();
    InitActionProperties();

    // Try loading controls.ini, otherwise use defaults
    if (!LoadProfileFromIni(controlsPath))
    {
        SetDefaultMappings();
    }

    // Open joystick/gamepad devices
    int numJoysticks = 0;
    SDL_JoystickID* joystickIds = SDL_GetJoysticks(&numJoysticks);
    if (joystickIds)
    {
        m_controllers.joysticksCount = 0;
        for (int i = 0; i < numJoysticks && i < MAX_JOYSTICKS; i++)
        {
            SDL_JoystickID id = joystickIds[i];
            if (SDL_IsGamepad(id))
            {
                m_controllers.controllers[i] = SDL_OpenGamepad(id);
                if (m_controllers.controllers[i])
                {
                    log_info("Opened gamepad %d: %s", i, SDL_GetGamepadName(m_controllers.controllers[i]));
                    m_controllers.joysticksCount++;
                }
            }
            else
            {
                m_controllers.joysticks[i] = SDL_OpenJoystick(id);
                if (m_controllers.joysticks[i])
                {
                    log_info("Opened joystick %d: %s", i, SDL_GetJoystickName(m_controllers.joysticks[i]));
                    m_controllers.joysticksCount++;
                }
            }
        }
        SDL_free(joystickIds);
    }

    DetectCombinedAxes();
    RemapPerGame();

    m_initialized = true;
    log_info("SdlInput initialized (gameType=%d, analogMax=%d, controllers=%d)",
             (int)gameType, m_jvsAnalogueMaxValue, m_controllers.joysticksCount);
    return 0;
}

void SdlInput::Shutdown()
{
    if (!m_initialized) return;

    for (int i = 0; i < MAX_JOYSTICKS; i++)
    {
        if (m_controllers.controllers[i])
        {
            SDL_CloseGamepad(m_controllers.controllers[i]);
            m_controllers.controllers[i] = nullptr;
        }
        if (m_controllers.joysticks[i])
        {
            SDL_CloseJoystick(m_controllers.joysticks[i]);
            m_controllers.joysticks[i] = nullptr;
        }
    }
    m_controllers.joysticksCount = 0;
    m_initialized = false;
}

// ============================================================
// InitJvsMappings (Layer 3: LogicalAction → JVS)
// ============================================================
void SdlInput::InitJvsMappings()
{
    for (int p = 0; p < MAX_ENTITIES; p++)
        for (int i = 0; i < NUM_LOGICAL_ACTIONS; i++)
            m_jvsMap[p][i] = {JVSActionMapping::JVS_CALL_NONE, NONE};

    GameType  gt  = getConfig()->gameType;
    GameID    gid = getConfig()->gameId;
    GameGroup grp = getConfig()->gameGroup;

    m_jvsMap[SYSTEM][LA_Test] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_TEST};

    for (int p = 1; p <= MAX_PLAYERS; p++)
    {
        m_jvsMap[p][LA_Start]   = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_START};
        m_jvsMap[p][LA_Service] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_SERVICE};
        m_jvsMap[p][LA_Coin]    = {JVSActionMapping::JVS_CALL_COIN,   COIN};

        int shootIdx = (p - 1) * 2;

        if (gt == DIGITAL)
        {
            m_jvsMap[p][LA_Up]      = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};
            m_jvsMap[p][LA_Down]    = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
            m_jvsMap[p][LA_Left]    = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_LEFT};
            m_jvsMap[p][LA_Right]   = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_RIGHT};
            m_jvsMap[p][LA_Button1] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_1};
            m_jvsMap[p][LA_Button2] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_2};
            m_jvsMap[p][LA_Button3] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_3};
        }
        else if (gt == SHOOTING)
        {
            m_jvsMap[p][LA_GunX]         = {JVSActionMapping::JVS_CALL_ANALOGUE, (JVSInput)(ANALOGUE_1 + shootIdx)};
            m_jvsMap[p][LA_GunY]         = {JVSActionMapping::JVS_CALL_ANALOGUE, (JVSInput)(ANALOGUE_2 + shootIdx)};
            m_jvsMap[p][LA_Trigger]      = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_1};
            m_jvsMap[p][LA_Reload]       = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_2};
            m_jvsMap[p][LA_GunButton]    = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_3};
            m_jvsMap[p][LA_ActionButton] = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_4};
            m_jvsMap[p][LA_ChangeButton] = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_5};
            m_jvsMap[p][LA_PedalLeft]    = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_LEFT};
            m_jvsMap[p][LA_PedalRight]   = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_RIGHT};
        }
    }

    if (gt == DRIVING)
    {
        m_jvsMap[PLAYER_1][LA_ViewChange] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
        m_jvsMap[PLAYER_2][LA_GearUp]     = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};
        m_jvsMap[PLAYER_2][LA_GearDown]   = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
        m_jvsMap[PLAYER_2][LA_Boost]      = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_1};
        m_jvsMap[PLAYER_1][LA_CardInsert] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};

        m_jvsMap[PLAYER_1][LA_Steer]         = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_Gas]           = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_Brake]         = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_3};
        m_jvsMap[PLAYER_1][LA_Gas_Digital]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_Brake_Digital] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_3};
        m_jvsMap[PLAYER_1][LA_Steer_Left]    = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_Steer_Right]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_Up]    = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};
        m_jvsMap[PLAYER_1][LA_Down]  = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
        m_jvsMap[PLAYER_1][LA_Left]  = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_LEFT};
        m_jvsMap[PLAYER_1][LA_Right] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_RIGHT};

        m_jvsMap[PLAYER_2][LA_Steer]         = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_5};
        m_jvsMap[PLAYER_2][LA_Gas]           = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_6};
        m_jvsMap[PLAYER_2][LA_Brake]         = {JVSActionMapping::JVS_CALL_ANALOGUE, (JVSInput)ANALOGUE_7};
        m_jvsMap[PLAYER_2][LA_Gas_Digital]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_6};
        m_jvsMap[PLAYER_2][LA_Brake_Digital] = {JVSActionMapping::JVS_CALL_ANALOGUE, (JVSInput)ANALOGUE_7};
        m_jvsMap[PLAYER_2][LA_Steer_Left]    = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_5};
        m_jvsMap[PLAYER_2][LA_Steer_Right]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_5};
    }
    else if (gt == GAME_TYPE_ABC)
    {
        m_jvsMap[PLAYER_1][LA_ABC_X]               = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_ABC_Left]             = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_ABC_Right]            = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_ABC_Y]               = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_ABC_Up]              = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_ABC_Down]            = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_Throttle]            = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_3};
        m_jvsMap[PLAYER_1][LA_Throttle_Accelerate] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_3};
        m_jvsMap[PLAYER_1][LA_Throttle_Slowdown]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_3};
        m_jvsMap[PLAYER_1][LA_GunTrigger]          = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_1};
        m_jvsMap[PLAYER_1][LA_MissileTrigger]      = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_2};
        m_jvsMap[PLAYER_1][LA_ClimaxSwitch]        = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_3};
    }
    else if (gt == MAHJONG)
    {
        m_jvsMap[PLAYER_1][LA_A] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_RIGHT};
        m_jvsMap[PLAYER_1][LA_B] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_LEFT};
        m_jvsMap[PLAYER_1][LA_C] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};
        m_jvsMap[PLAYER_1][LA_D] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
        m_jvsMap[PLAYER_1][LA_E] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_1};
        m_jvsMap[PLAYER_1][LA_F] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_2};
        m_jvsMap[PLAYER_1][LA_G] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_3};
        m_jvsMap[PLAYER_1][LA_H] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_4};
        m_jvsMap[PLAYER_1][LA_I] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_5};
        m_jvsMap[PLAYER_1][LA_J] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_6};
        m_jvsMap[PLAYER_1][LA_K] = {JVSActionMapping::JVS_CALL_SWITCH, (JVSInput)BUTTON_7};
        m_jvsMap[PLAYER_2][LA_L]       = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_RIGHT};
        m_jvsMap[PLAYER_2][LA_M]       = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_LEFT};
        m_jvsMap[PLAYER_2][LA_N]       = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};
        m_jvsMap[PLAYER_2][LA_Chi]     = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
        m_jvsMap[PLAYER_2][LA_Pon]     = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_1};
        m_jvsMap[PLAYER_2][LA_Kan]     = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_2};
        m_jvsMap[PLAYER_2][LA_Reach]   = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_3};
        m_jvsMap[PLAYER_2][LA_Agari]   = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_4};
        m_jvsMap[PLAYER_2][LA_Cancel]  = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_5};
        m_jvsMap[PLAYER_2][LA_CardInsert] = {JVSActionMapping::JVS_CALL_SWITCH, (JVSInput)BUTTON_7};
    }

    m_jvsMap[PLAYER_1][LA_Card1Insert] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_4};
    m_jvsMap[PLAYER_2][LA_Card2Insert] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_4};
}

// ============================================================
// RemapPerGame
// ============================================================
void SdlInput::RemapPerGame()
{
    GameID    gid = getConfig()->gameId;
    GameGroup grp = getConfig()->gameGroup;

    if (gid == R_TUNED)
    {
        m_jvsMap[PLAYER_1][LA_BoostRight] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_RIGHT};
        m_jvsMap[PLAYER_1][LA_CardInsert] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_UP};
    }
    else if (grp == GROUP_HUMMER)
    {
        m_jvsMap[PLAYER_2][LA_Boost] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_DOWN};
    }
    else if (grp == GROUP_ID4_EXP || grp == GROUP_ID4_JAP || grp == GROUP_ID5)
    {
        m_jvsMap[PLAYER_1][LA_ViewChange] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_1};
    }
    else if (gid == LETS_GO_JUNGLE || gid == LETS_GO_JUNGLE_REVA)
    {
        m_jvsMap[PLAYER_1][LA_GunX] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_GunY] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_2][LA_GunX] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_2][LA_GunY] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
    }
    else if (gid == HARLEY_DAVIDSON)
    {
        m_jvsMap[PLAYER_1][LA_Steer]         = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_Steer_Left]    = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_Steer_Right]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_2};
        m_jvsMap[PLAYER_1][LA_Gas]           = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_Gas_Digital]   = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_1};
        m_jvsMap[PLAYER_1][LA_Brake]         = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_4};
        m_jvsMap[PLAYER_1][LA_Brake_Digital] = {JVSActionMapping::JVS_CALL_ANALOGUE, ANALOGUE_4};
        m_jvsMap[PLAYER_1][LA_MusicChange]   = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_1};
        m_jvsMap[PLAYER_1][LA_ViewChange]    = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_2};
        m_jvsMap[PLAYER_1][LA_GearUp]        = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_3};
        m_jvsMap[PLAYER_1][LA_GearDown]      = {JVSActionMapping::JVS_CALL_SWITCH,   BUTTON_4};
    }
    else if (gid == PRIMEVAL_HUNT)
    {
        m_jvsMap[PLAYER_1][LA_Reload]    = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_2};
        m_jvsMap[PLAYER_1][LA_GunButton] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_3};
        m_jvsMap[PLAYER_2][LA_Reload]    = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_2};
        m_jvsMap[PLAYER_2][LA_GunButton] = {JVSActionMapping::JVS_CALL_SWITCH, BUTTON_3};
    }
}

// ============================================================
// InitActionProperties
// ============================================================
void SdlInput::InitActionProperties()
{
    for (int p = 0; p < MAX_ENTITIES; p++)
    {
        for (int i = 0; i < NUM_LOGICAL_ACTIONS; i++)
        {
            m_actionProps[p][i].isCentering = false;
            m_actionProps[p][i].isCombinedAxis = false;
            m_actionProps[p][i].deadzone = 100;
        }
        m_actionProps[p][LA_Steer].isCentering = true;
    }
    m_actionProps[PLAYER_1][LA_ABC_X].isCentering = true;
    m_actionProps[PLAYER_1][LA_ABC_Y].isCentering = true;
    m_actionProps[PLAYER_1][LA_Throttle].isCentering = true;
}

// ============================================================
// SetDefaultMappings
// ============================================================
void SdlInput::SetDefaultMappings()
{
    log_warn("controls.ini not found. Applying default mappings...");

    for (size_t i = 0; i < g_defaultCommonBindingsSize; i++)
        AddBinding(g_defaultCommonBindings[i]);

    const ControlBinding* bindings = nullptr;
    size_t count = 0;
    GameType gt = getConfig()->gameType;

    if (gt == DIGITAL)           { bindings = g_defaultDigitalBindings;  count = g_defaultDigitalBindingsSize; }
    else if (gt == DRIVING)      { bindings = g_defaultDrivingBindings;  count = g_defaultDrivingBindingsSize; }
    else if (gt == GAME_TYPE_ABC){ bindings = g_defaultAbcBindings;      count = g_defaultAbcBindingsSize; }
    else if (gt == SHOOTING)     { bindings = g_defaultShootingBindings; count = g_defaultShootingBindingsSize; }
    else if (gt == MAHJONG)      { bindings = g_defaultMahjongBindings;  count = g_defaultMahjongBindingsSize; }

    if (bindings)
        for (size_t i = 0; i < count; i++)
            AddBinding(bindings[i]);
}

// ============================================================
// AddBinding
// ============================================================
void SdlInput::AddBinding(ControlBinding binding)
{
    // Non-axis types: single binding per input
    if (binding.type != INPUT_TYPE_JOY_AXIS && binding.type != INPUT_TYPE_GAMEPAD_AXIS && binding.type != INPUT_TYPE_JOY_HAT)
    {
        ControlBinding* existing = nullptr;
        switch (binding.type)
        {
        case INPUT_TYPE_KEY:
            if (binding.sdlId >= 0 && binding.sdlId < SDL_SCANCODE_COUNT)
                existing = &m_keyBindings[binding.sdlId];
            break;
        case INPUT_TYPE_MOUSE_BUTTON:
            if (binding.sdlId >= 0 && binding.sdlId < MAX_MOUSE_BUTTONS)
                existing = &m_mouseButtonBindings[binding.sdlId];
            break;
        case INPUT_TYPE_MOUSE_AXIS:
            if (binding.sdlId >= 0 && binding.sdlId < 2)
                existing = &m_mouseAxisBindings[binding.sdlId];
            break;
        case INPUT_TYPE_JOY_BUTTON:
            if (binding.deviceIndex < MAX_JOYSTICKS && binding.sdlId < MAX_JOY_BUTTONS)
                existing = &m_joyButtonBindings[binding.deviceIndex][binding.sdlId];
            break;
        case INPUT_TYPE_GAMEPAD_BUTTON:
            if (binding.deviceIndex < MAX_JOYSTICKS && binding.sdlId < SDL_GAMEPAD_BUTTON_COUNT)
                existing = &m_controllerButtonBindings[binding.deviceIndex][binding.sdlId];
            break;
        default: break;
        }
        if (existing)
        {
            if (existing->type != INPUT_TYPE_NONE && existing->action != binding.action)
                log_warn("Input binding conflict for action %d vs %d", existing->action, binding.action);
            else
                *existing = binding;
        }
        return;
    }

    // Hat bindings
    if (binding.type == INPUT_TYPE_JOY_HAT)
    {
        if (binding.deviceIndex < MAX_JOYSTICKS && binding.sdlId < MAX_JOY_HATS)
        {
            HatBinding* hat = &m_joyHatBindings[binding.deviceIndex][binding.sdlId];
            ControlBinding* slot = nullptr;
            if (binding.axisThreshold == SDL_HAT_UP)         slot = &hat->up;
            else if (binding.axisThreshold == SDL_HAT_DOWN)  slot = &hat->down;
            else if (binding.axisThreshold == SDL_HAT_LEFT)  slot = &hat->left;
            else if (binding.axisThreshold == SDL_HAT_RIGHT) slot = &hat->right;
            if (slot) *slot = binding;
        }
        return;
    }

    // Axis bindings (allow 2 per physical axis)
    BindingPair* pair = nullptr;
    if (binding.type == INPUT_TYPE_JOY_AXIS)
    {
        if (binding.deviceIndex < MAX_JOYSTICKS && binding.sdlId < MAX_JOY_AXES)
            pair = &m_joyAxisBindings[binding.deviceIndex][binding.sdlId];
    }
    else if (binding.type == INPUT_TYPE_GAMEPAD_AXIS)
    {
        if (binding.deviceIndex < MAX_JOYSTICKS && binding.sdlId < SDL_GAMEPAD_AXIS_COUNT)
            pair = &m_controllerAxisBindings[binding.deviceIndex][binding.sdlId];
    }

    if (pair)
    {
        if (pair->bindings[0].type == INPUT_TYPE_NONE)
            pair->bindings[0] = binding;
        else if (pair->bindings[1].type == INPUT_TYPE_NONE)
            pair->bindings[1] = binding;
        else
            log_warn("Cannot bind more than 2 actions to same axis");
    }
}

// ============================================================
// AddActionToDirtyList
// ============================================================
void SdlInput::AddActionToDirtyList(JVSPlayer player, LogicalAction action)
{
    for (int i = 0; i < m_numChangedActions; i++)
        if (m_changedActions[i].player == player && m_changedActions[i].action == action)
            return;
    if (m_numChangedActions < (NUM_LOGICAL_ACTIONS * MAX_ENTITIES))
        m_changedActions[m_numChangedActions++] = {player, action};
}

// ============================================================
// Device ID helpers
// ============================================================
int SdlInput::GetControllerID(SDL_JoystickID instanceId)
{
    for (int i = 0; i < MAX_JOYSTICKS; i++)
        if (m_controllers.controllers[i])
        {
            SDL_Joystick* joy = SDL_GetGamepadJoystick(m_controllers.controllers[i]);
            if (joy && SDL_GetJoystickID(joy) == instanceId) return i;
        }
    return -1;
}

int SdlInput::GetJoystickID(SDL_JoystickID instanceId)
{
    for (int i = 0; i < MAX_JOYSTICKS; i++)
        if (m_controllers.joysticks[i])
            if (SDL_GetJoystickID(m_controllers.joysticks[i]) == instanceId) return i;
    return -1;
}

// ============================================================
// ProcessEvent
// ============================================================
void SdlInput::ProcessEvent(const SDL_Event* e)
{
    switch (e->type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    {
        if (e->key.scancode >= 0 && e->key.scancode < SDL_SCANCODE_COUNT)
        {
            ControlBinding* binding = &m_keyBindings[e->key.scancode];
            if (binding->type != INPUT_TYPE_NONE)
            {
                bool isActive = (e->type == SDL_EVENT_KEY_DOWN);
                if (m_actionStates[binding->player][binding->action].isActive != isActive)
                {
                    m_actionStates[binding->player][binding->action].isActive = isActive;
                    AddActionToDirtyList(binding->player, binding->action);
                }
            }
        }
        break;
    }

    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
    {
        int devIdx = GetJoystickID(e->jbutton.which);
        if (devIdx != -1 && e->jbutton.button < MAX_JOY_BUTTONS)
        {
            ControlBinding* binding = &m_joyButtonBindings[devIdx][e->jbutton.button];
            if (binding->type != INPUT_TYPE_NONE)
            {
                bool isActive = (e->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN);
                if (m_actionStates[binding->player][binding->action].isActive != isActive)
                {
                    m_actionStates[binding->player][binding->action].isActive = isActive;
                    AddActionToDirtyList(binding->player, binding->action);
                }
            }
        }
        break;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    {
        int devIdx = GetControllerID(e->gbutton.which);
        if (devIdx != -1 && e->gbutton.button < SDL_GAMEPAD_BUTTON_COUNT)
        {
            ControlBinding* binding = &m_controllerButtonBindings[devIdx][e->gbutton.button];
            if (binding->type != INPUT_TYPE_NONE)
            {
                bool isActive = (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                if (m_actionStates[binding->player][binding->action].isActive != isActive)
                {
                    m_actionStates[binding->player][binding->action].isActive = isActive;
                    AddActionToDirtyList(binding->player, binding->action);
                }
            }
        }
        break;
    }

    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
    {
        int devIdx = GetJoystickID(e->jaxis.which);
        if (devIdx != -1 && e->jaxis.axis < MAX_JOY_AXES)
        {
            BindingPair* pair = &m_joyAxisBindings[devIdx][e->jaxis.axis];
            for (int i = 0; i < 2; i++)
            {
                ControlBinding* binding = &pair->bindings[i];
                if (binding->type == INPUT_TYPE_NONE) continue;

                int deadzone = m_actionProps[binding->player][binding->action].deadzone;
                ActionState* state = &m_actionStates[binding->player][binding->action];
                bool changed = false;

                if (binding->axisMode == AXIS_MODE_DIGITAL)
                {
                    bool active = (abs(e->jaxis.value) > deadzone)
                        ? ((binding->axisThreshold > 0) ? (e->jaxis.value > 16384) : (e->jaxis.value < -16384))
                        : false;
                    if (state->isActive != active) { state->isActive = active; changed = true; }
                }
                else
                {
                    float newVal;
                    if (m_actionProps[binding->player][binding->action].isCentering)
                        newVal = (abs(e->jaxis.value) < deadzone) ? 0.5f : (float)(e->jaxis.value + 32768) / 65535.0f;
                    else
                        newVal = (abs(e->jaxis.value - (-32768)) < deadzone) ? 0.0f : (float)(e->jaxis.value + 32768) / 65535.0f;
                    if (binding->isInverted) newVal = 1.0f - newVal;

                    if (binding->axisMode == AXIS_MODE_POSITIVE_HALF)
                        { if (fabsf(state->positiveContribution - newVal) > 0.001f) { state->positiveContribution = newVal; changed = true; } }
                    else if (binding->axisMode == AXIS_MODE_NEGATIVE_HALF)
                        { if (fabsf(state->negativeContribution - newVal) > 0.001f) { state->negativeContribution = newVal; changed = true; } }
                    else
                        { if (fabsf(state->analogValue - newVal) > 0.001f) { state->analogValue = newVal; changed = true; } }
                }
                if (changed) AddActionToDirtyList(binding->player, binding->action);
            }
        }
        break;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
        int devIdx = GetControllerID(e->gaxis.which);
        if (devIdx != -1 && e->gaxis.axis < SDL_GAMEPAD_AXIS_COUNT)
        {
            BindingPair* pair = &m_controllerAxisBindings[devIdx][e->gaxis.axis];
            for (int i = 0; i < 2; i++)
            {
                ControlBinding* binding = &pair->bindings[i];
                if (binding->type == INPUT_TYPE_NONE) continue;

                int deadzone = m_actionProps[binding->player][binding->action].deadzone;
                ActionState* state = &m_actionStates[binding->player][binding->action];
                bool changed = false;

                if (binding->axisMode == AXIS_MODE_DIGITAL)
                {
                    bool active = (abs(e->gaxis.value) > deadzone)
                        ? ((binding->axisThreshold > 0) ? (e->gaxis.value > 16384) : (e->gaxis.value < -16384))
                        : false;
                    if (state->isActive != active) { state->isActive = active; changed = true; }
                }
                else
                {
                    bool isTrigger = (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || e->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
                    float newVal;
                    if (m_actionProps[binding->player][binding->action].isCentering && !isTrigger)
                        newVal = (abs(e->gaxis.value) < deadzone) ? 0.5f : (float)(e->gaxis.value + 32768) / 65535.0f;
                    else
                        newVal = (abs(e->gaxis.value) < deadzone) ? 0.0f : (float)e->gaxis.value / 32767.0f;
                    if (binding->isInverted) newVal = 1.0f - newVal;

                    if (binding->axisMode == AXIS_MODE_POSITIVE_HALF)
                        { if (fabsf(state->positiveContribution - newVal) > 0.001f) { state->positiveContribution = newVal; changed = true; } }
                    else if (binding->axisMode == AXIS_MODE_NEGATIVE_HALF)
                        { if (fabsf(state->negativeContribution - newVal) > 0.001f) { state->negativeContribution = newVal; changed = true; } }
                    else
                        { if (fabsf(state->analogValue - newVal) > 0.001f) { state->analogValue = newVal; changed = true; } }
                }
                if (changed) AddActionToDirtyList(binding->player, binding->action);
            }
        }
        break;
    }

    case SDL_EVENT_JOYSTICK_HAT_MOTION:
    {
        int devIdx = GetJoystickID(e->jhat.which);
        if (devIdx != -1 && e->jhat.hat < MAX_JOY_HATS)
        {
            HatBinding* hat = &m_joyHatBindings[devIdx][e->jhat.hat];
            ControlBinding* dirs[] = {&hat->up, &hat->down, &hat->left, &hat->right};
            for (int i = 0; i < 4; i++)
            {
                if (dirs[i]->type == INPUT_TYPE_NONE) continue;
                bool active = (e->jhat.value & dirs[i]->axisThreshold) != 0;
                ActionState* state = &m_actionStates[dirs[i]->player][dirs[i]->action];
                if (state->isActive != active) { state->isActive = active; AddActionToDirtyList(dirs[i]->player, dirs[i]->action); }
            }
        }
        break;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        if (e->button.button < MAX_MOUSE_BUTTONS)
        {
            ControlBinding* binding = &m_mouseButtonBindings[e->button.button];
            if (binding->type != INPUT_TYPE_NONE)
            {
                bool isActive = (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                GameID gid = getConfig()->gameId;

                if (m_actionStates[binding->player][binding->action].isActive != isActive)
                {
                    m_actionStates[binding->player][binding->action].isActive = isActive;

                    // Rambo/TooSpicy: right-click triggers reload with off-screen gun
                    if (e->button.button == SDL_BUTTON_RIGHT && (gid == RAMBO || gid == RAMBO_CHINA || gid == TOO_SPICY) && isActive)
                    {
                        m_actionStates[binding->player][LA_GunX].analogValue = -1;
                        AddActionToDirtyList(binding->player, LA_GunX);
                        m_actionStates[binding->player][LA_GunY].analogValue = -1;
                        AddActionToDirtyList(binding->player, LA_GunY);
                        m_actionStates[binding->player][LA_Reload].isActive = isActive;
                        AddActionToDirtyList(binding->player, LA_Reload);
                    }
                    else
                    {
                        AddActionToDirtyList(binding->player, binding->action);
                    }
                }
            }
        }
        break;
    }

    case SDL_EVENT_MOUSE_MOTION:
    {
        float mX = e->motion.x;
        float mY = e->motion.y;

        // Map mouse coords to 0.0-1.0 range using viewport destination
        float posX = 0.0f, posY = 0.0f;
        if (m_dest.W > 0 && m_dest.H > 0)
        {
            if (mX <= m_dest.X)        posX = 0;
            else if (mX >= (m_dest.W + m_dest.X - 5)) posX = (float)m_dest.W;
            else                       posX = mX - m_dest.X;

            if (mY <= m_dest.Y)        posY = 0;
            else if (mY >= (m_dest.H + m_dest.Y - 5)) posY = (float)m_dest.H;
            else                       posY = mY - m_dest.Y;

            posX /= m_dest.W;
            posY /= m_dest.H;
        }
        else
        {
            // Fallback: use config width/height
            int w = getConfig()->width;
            int h = getConfig()->height;
            if (w > 0 && h > 0)
            {
                posX = mX / (float)w;
                posY = mY / (float)h;
            }
        }
        if (posX < 0) posX = 0; if (posX > 1.0f) posX = 1.0f;
        if (posY < 0) posY = 0; if (posY > 1.0f) posY = 1.0f;

        // Edge-of-screen reload for shooting games
        GameType gt = getConfig()->gameType;
        GameID   gid = getConfig()->gameId;
        if (gt == SHOOTING && (posX <= 0.01f || posX >= 0.99f || posY <= 0.01f || posY >= 0.99f))
        {
            if (gid == RAMBO || gid == RAMBO_CHINA || gid == TOO_SPICY)
            {
                ControlBinding* bx = &m_mouseAxisBindings[0];
                ControlBinding* by = &m_mouseAxisBindings[1];
                if (bx->type != INPUT_TYPE_NONE) { m_actionStates[bx->player][LA_GunX].analogValue = -1; AddActionToDirtyList(bx->player, LA_GunX); }
                if (by->type != INPUT_TYPE_NONE) { m_actionStates[by->player][LA_GunY].analogValue = -1; AddActionToDirtyList(by->player, LA_GunY); }
            }
            m_actionStates[PLAYER_1][LA_Reload].isActive = true;
            AddActionToDirtyList(PLAYER_1, LA_Reload);
            break;
        }
        else
        {
            m_actionStates[PLAYER_1][LA_Reload].isActive = false;
            AddActionToDirtyList(PLAYER_1, LA_Reload);
        }

        ControlBinding* bindX = &m_mouseAxisBindings[0];
        ControlBinding* bindY = &m_mouseAxisBindings[1];
        if (bindX->type != INPUT_TYPE_NONE)
        {
            ActionState* st = &m_actionStates[bindX->player][bindX->action];
            float val = bindX->isInverted ? (1.0f - posX) : posX;
            if (fabsf(st->analogValue - val) > 0.001f) { st->analogValue = val; AddActionToDirtyList(bindX->player, bindX->action); }
        }
        if (bindY->type != INPUT_TYPE_NONE)
        {
            ActionState* st = &m_actionStates[bindY->player][bindY->action];
            float val = bindY->isInverted ? (1.0f - posY) : posY;
            if (fabsf(st->analogValue - val) > 0.001f) { st->analogValue = val; AddActionToDirtyList(bindY->player, bindY->action); }
        }
        break;
    }

    default:
        break;
    }
}

#include "cardreader.h"  // for g_triggerInsertKey

// ============================================================
// ProcessChangedActions
// ============================================================
void SdlInput::ProcessChangedActions()
{
    JvsBoard* jvs = LindberghDevice::Instance().GetJvsBoard();
    if (!jvs) return;

    GameGroup grp = getConfig()->gameGroup;

    for (int i = 0; i < m_numChangedActions; i++)
    {
        JVSPlayer player = m_changedActions[i].player;
        LogicalAction actionId = m_changedActions[i].action;
        JVSActionMapping* map = &m_jvsMap[player][actionId];
        ActionState* state = &m_actionStates[player][actionId];

        switch (map->call_type)
        {
        case JVSActionMapping::JVS_CALL_SWITCH:
            // ID4/ID5: CardInsert sets trigger flag instead of JVS switch
            if ((grp == GROUP_ID4_EXP || grp == GROUP_ID4_JAP || grp == GROUP_ID5) && actionId == LA_CardInsert)
            {
                g_triggerInsertKey = state->isActive;
                break;
            }
            jvs->SetSwitch(player, map->jvsInput, state->isActive);
            break;

        case JVSActionMapping::JVS_CALL_ANALOGUE:
        {
            // Digital actions controlling analog JVS inputs
            switch (actionId)
            {
            case LA_Gas_Digital:
            case LA_Brake_Digital:
                state->analogValue = state->isActive ? 1.0f : 0.0f;
                break;
            case LA_Steer_Left:
            case LA_ABC_Left:
            case LA_ABC_Up:
            case LA_Throttle_Slowdown:
                state->analogValue = state->isActive ? 0.0f : 0.5f;
                break;
            case LA_Steer_Right:
            case LA_ABC_Right:
            case LA_ABC_Down:
            case LA_Throttle_Accelerate:
                state->analogValue = state->isActive ? 1.0f : 0.5f;
                break;
            default:
                break;
            }
            jvs->SetAnalogue(map->jvsInput, (int)(state->analogValue * m_jvsAnalogueMaxValue));
            break;
        }

        case JVSActionMapping::JVS_CALL_COIN:
            if (state->isActive)
                jvs->IncrementCoin(player, 1);
            break;

        case JVSActionMapping::JVS_CALL_NONE:
            break;
        }
    }
    m_numChangedActions = 0;
}

// ============================================================
// UpdateCombinedAxes
// ============================================================
void SdlInput::UpdateCombinedAxes()
{
    for (int p = PLAYER_1; p <= MAX_PLAYERS; p++)
    {
        for (int i = 0; i < NUM_LOGICAL_ACTIONS; i++)
        {
            if (m_actionProps[p][i].isCombinedAxis)
            {
                ActionState* state = &m_actionStates[p][i];
                float val = 0.5f + (state->positiveContribution * 0.5f) - (state->negativeContribution * 0.5f);
                if (val > 1.0f) val = 1.0f;
                if (val < 0.0f) val = 0.0f;
                if (fabsf(state->analogValue - val) > 0.001f)
                {
                    state->analogValue = val;
                    AddActionToDirtyList((JVSPlayer)p, (LogicalAction)i);
                }
            }
        }
    }
}

// ============================================================
// UpdateGunShake (HOD4)
// ============================================================
void SdlInput::UpdateGunShake()
{
    JvsBoard* jvs = LindberghDevice::Instance().GetJvsBoard();
    if (!jvs) return;

    for (int p = PLAYER_1; p <= PLAYER_2; p++)
    {
        int curX = (int)(m_jvsAnalogueMaxValue * m_actionStates[p][LA_GunX].analogValue);
        int curY = (int)(m_jvsAnalogueMaxValue * m_actionStates[p][LA_GunY].analogValue);
        int dx = curX - m_lastGunX[p];
        int dy = curY - m_lastGunY[p];
        int dirX = (dx > 2) ? 1 : ((dx < -2) ? -1 : 0);
        int dirY = (dy > 2) ? 1 : ((dy < -2) ? -1 : 0);

        if (dirX != 0 && dirX == -m_lastGunXDir[p]) m_shakeValue[p] += abs(dx) * m_shakeIncreaseRate;
        if (dirY != 0 && dirY == -m_lastGunYDir[p]) m_shakeValue[p] += abs(dy) * m_shakeIncreaseRate;

        m_shakeValue[p] *= m_shakeDecayRate;
        if (m_shakeValue[p] < 1.0f) m_shakeValue[p] = 0.0f;
        if (m_shakeValue[p] > 512.0f) m_shakeValue[p] = 512.0f;

        int jvsOut = 512 + (int)m_shakeValue[p];
        if (p == PLAYER_1)      { jvs->SetAnalogue(ANALOGUE_5, jvsOut); jvs->SetAnalogue(ANALOGUE_6, jvsOut); }
        else if (p == PLAYER_2) { jvs->SetAnalogue((JVSInput)ANALOGUE_7, jvsOut); jvs->SetAnalogue((JVSInput)ANALOGUE_8, jvsOut); }

        m_lastGunX[p] = curX; m_lastGunY[p] = curY;
        if (dirX != 0) m_lastGunXDir[p] = dirX;
        if (dirY != 0) m_lastGunYDir[p] = dirY;
    }
}

// ============================================================
// SetViewDest
// ============================================================
void SdlInput::SetViewDest(int x, int y, int w, int h)
{
    m_dest.X = x; m_dest.Y = y; m_dest.W = w; m_dest.H = h;
}

// ============================================================
// DetectCombinedAxes
// ============================================================
void SdlInput::ScanAxisBindings(BindingPair* pair, bool hasFullAxis[], bool hasHalfAxis[],
                                bool hasPositive[][NUM_LOGICAL_ACTIONS],
                                bool hasNegative[][NUM_LOGICAL_ACTIONS])
{
    for (int k = 0; k < 2; k++)
    {
        ControlBinding* b = &pair->bindings[k];
        if (b->type == INPUT_TYPE_NONE) continue;
        int act = b->action;
        int p   = b->player;
        if (b->axisMode == AXIS_MODE_FULL) hasFullAxis[act] = true;
        if (b->axisMode == AXIS_MODE_POSITIVE_HALF) { hasHalfAxis[act] = true; hasPositive[p][act] = true; }
        if (b->axisMode == AXIS_MODE_NEGATIVE_HALF) { hasHalfAxis[act] = true; hasNegative[p][act] = true; }
    }
}

void SdlInput::DetectCombinedAxes()
{
    bool hasFullAxis[NUM_LOGICAL_ACTIONS] = {};
    bool hasHalfAxis[NUM_LOGICAL_ACTIONS] = {};
    bool hasPositive[MAX_ENTITIES][NUM_LOGICAL_ACTIONS] = {};
    bool hasNegative[MAX_ENTITIES][NUM_LOGICAL_ACTIONS] = {};

    for (int i = 0; i < MAX_JOYSTICKS; i++)
    {
        for (int j = 0; j < MAX_JOY_AXES; j++)
            ScanAxisBindings(&m_joyAxisBindings[i][j], hasFullAxis, hasHalfAxis, hasPositive, hasNegative);
        for (int j = 0; j < SDL_GAMEPAD_AXIS_COUNT; j++)
            ScanAxisBindings(&m_controllerAxisBindings[i][j], hasFullAxis, hasHalfAxis, hasPositive, hasNegative);
    }

    for (int p = 0; p < MAX_ENTITIES; p++)
        for (int i = 0; i < NUM_LOGICAL_ACTIONS; i++)
            if (hasPositive[p][i] && hasNegative[p][i])
                m_actionProps[p][i].isCombinedAxis = true;
}

// ============================================================
// LoadProfileFromIni (simplified)
// ============================================================
bool SdlInput::ParseActionKey(const char* key, JVSPlayer* outPlayer, LogicalAction* outAction)
{
    char genericKey[64] = {};
    int pNum = 0;
    if (sscanf(key, "P%d_%63s", &pNum, genericKey) == 2)
    {
        if (pNum >= 1 && pNum <= MAX_PLAYERS) *outPlayer = (JVSPlayer)pNum;
        else return false;
    }
    else
    {
        *outPlayer = PLAYER_1;
        strncpy(genericKey, key, 63);
        if (strcmp(genericKey, "Test") == 0) *outPlayer = SYSTEM;
    }
    for (int i = 0; i < g_numActionNames; i++)
    {
        if (strcmp(g_actionNameMap[i].name, genericKey) == 0)
        {
            *outAction = g_actionNameMap[i].action;
            return true;
        }
    }
    return false;
}

JVSPlayer SdlInput::FixPlayerForAction(LogicalAction action, int player)
{
    GameID gid = getConfig()->gameId;
    if (action == LA_Boost) return PLAYER_2;
    if ((action == LA_GearUp || action == LA_GearDown) && gid != HARLEY_DAVIDSON) return PLAYER_2;
    return (JVSPlayer)player;
}

bool SdlInput::IsMahjongP2(LogicalAction action)
{
    switch (action) {
    case LA_L: case LA_M: case LA_N: case LA_Chi: case LA_Pon:
    case LA_Kan: case LA_Reach: case LA_Agari: case LA_Cancel: case LA_CardInsert:
        return true;
    default: return false;
    }
}

void SdlInput::ParseSdlSource(const char* token, JVSPlayer player, LogicalAction action)
{
    ControlBinding map = {};
    map.action = action;
    map.player = player;
    map.type = INPUT_TYPE_NONE;

    char buf[128];
    strncpy(buf, token, 127); buf[127] = '\0';

    char* inv = strstr(buf, "_INVERTED");
    if (inv) { map.isInverted = true; *inv = '\0'; }

    if (strncmp(buf, "KEY_", 4) == 0)
    {
        map.type = INPUT_TYPE_KEY;
        if (_stricmp(buf + 4, "Comma") == 0) map.sdlId = SDL_SCANCODE_COMMA;
        else map.sdlId = SDL_GetScancodeFromName(buf + 4);
    }
    else if (strncmp(buf, "GC", 2) == 0)
    {
        int devId; char typeStr[32], nameStr[64];
        if (sscanf(buf, "GC%d_%31[^_]_%63s", &devId, typeStr, nameStr) == 3)
        {
            map.deviceIndex = devId;
            if (strcmp(typeStr, "BUTTON") == 0)
            {
                map.type = INPUT_TYPE_GAMEPAD_BUTTON;
                map.sdlId = SDL_GetGamepadButtonFromString(nameStr);
            }
            else if (strcmp(typeStr, "AXIS") == 0)
            {
                map.type = INPUT_TYPE_GAMEPAD_AXIS;
                map.sdlId = SDL_GetGamepadAxisFromString(nameStr);
                if (strstr(buf, "_POSITIVE_HALF"))      map.axisMode = AXIS_MODE_POSITIVE_HALF;
                else if (strstr(buf, "_NEGATIVE_HALF"))  map.axisMode = AXIS_MODE_NEGATIVE_HALF;
                else if (strstr(buf, "_POSITIVE"))       { map.axisMode = AXIS_MODE_DIGITAL; map.axisThreshold = 1; }
                else if (strstr(buf, "_NEGATIVE"))       { map.axisMode = AXIS_MODE_DIGITAL; map.axisThreshold = -1; }
                else                                     map.axisMode = AXIS_MODE_FULL;
            }
        }
    }
    else if (strncmp(buf, "JOY", 3) == 0)
    {
        int devId, compId; char dirStr[32];
        if (sscanf(buf, "JOY%d_BUTTON_%d", &devId, &compId) == 2)
            { map.type = INPUT_TYPE_JOY_BUTTON; map.deviceIndex = devId; map.sdlId = compId; }
        else if (sscanf(buf, "JOY%d_AXIS_%d", &devId, &compId) == 2)
        {
            map.type = INPUT_TYPE_JOY_AXIS; map.deviceIndex = devId; map.sdlId = compId;
            if (strstr(buf, "_POSITIVE_HALF"))      map.axisMode = AXIS_MODE_POSITIVE_HALF;
            else if (strstr(buf, "_NEGATIVE_HALF"))  map.axisMode = AXIS_MODE_NEGATIVE_HALF;
            else if (strstr(buf, "_POSITIVE"))       { map.axisMode = AXIS_MODE_DIGITAL; map.axisThreshold = 1; }
            else if (strstr(buf, "_NEGATIVE"))       { map.axisMode = AXIS_MODE_DIGITAL; map.axisThreshold = -1; }
            else                                     map.axisMode = AXIS_MODE_FULL;
        }
        else if (sscanf(buf, "JOY%d_HAT%d_%31s", &devId, &compId, dirStr) == 3)
        {
            map.type = INPUT_TYPE_JOY_HAT; map.deviceIndex = devId; map.sdlId = compId;
            if (strcmp(dirStr, "UP") == 0)         map.axisThreshold = SDL_HAT_UP;
            else if (strcmp(dirStr, "DOWN") == 0)  map.axisThreshold = SDL_HAT_DOWN;
            else if (strcmp(dirStr, "LEFT") == 0)  map.axisThreshold = SDL_HAT_LEFT;
            else if (strcmp(dirStr, "RIGHT") == 0) map.axisThreshold = SDL_HAT_RIGHT;
        }
    }
    else if (strncmp(buf, "MOUSE_", 6) == 0)
    {
        const char* mt = buf + 6;
        if (strcmp(mt, "AXIS_X") == 0)             { map.type = INPUT_TYPE_MOUSE_AXIS;   map.sdlId = 0; }
        else if (strcmp(mt, "AXIS_Y") == 0)        { map.type = INPUT_TYPE_MOUSE_AXIS;   map.sdlId = 1; }
        else if (strcmp(mt, "LEFT_BUTTON") == 0)   { map.type = INPUT_TYPE_MOUSE_BUTTON; map.sdlId = SDL_BUTTON_LEFT; }
        else if (strcmp(mt, "RIGHT_BUTTON") == 0)  { map.type = INPUT_TYPE_MOUSE_BUTTON; map.sdlId = SDL_BUTTON_RIGHT; }
        else if (strcmp(mt, "MIDDLE_BUTTON") == 0) { map.type = INPUT_TYPE_MOUSE_BUTTON; map.sdlId = SDL_BUTTON_MIDDLE; }
    }

    if (map.type != INPUT_TYPE_NONE)
        AddBinding(map);
}

bool SdlInput::LoadProfileFromIni(const char* controlsPath)
{
    IniParser ini;
    if (!ini.Load(controlsPath))
        return false;

    log_info("Loaded controls from %s", controlsPath);
    GameType gt = getConfig()->gameType;

    // Load common section
    const IniSection* common = ini.GetSection("Common");
    if (common)
    {
        for (const auto& pair : common->pairs)
        {
            JVSPlayer player; LogicalAction action;
            if (!ParseActionKey(pair.key.c_str(), &player, &action)) continue;
            if (player != SYSTEM) player = FixPlayerForAction(action, player);

            // Split value by comma, parse each token
            char* dup = _strdup(pair.value.c_str());
            char* tok = strtok(dup, ", ");
            while (tok) { ParseSdlSource(tok, player, action); tok = strtok(nullptr, ", "); }
            free(dup);
        }
    }

    // Load game-type section
    const char* sectionName = nullptr;
    if (gt == DRIVING)        sectionName = "Driving";
    else if (gt == DIGITAL)   sectionName = "Digital";
    else if (gt == SHOOTING)  sectionName = "Shooting";
    else if (gt == GAME_TYPE_ABC) sectionName = "ABC";
    else if (gt == MAHJONG)   sectionName = "Mahjong";

    if (sectionName)
    {
        const IniSection* sec = ini.GetSection(sectionName);
        if (sec)
        {
            for (const auto& pair : sec->pairs)
            {
                JVSPlayer player; LogicalAction action;
                if (!ParseActionKey(pair.key.c_str(), &player, &action)) continue;
                if (player != SYSTEM && action != LA_Card1Insert && action != LA_Card2Insert)
                    player = FixPlayerForAction(action, player);
                if (action == LA_Card2Insert && gt == DIGITAL)
                    player = PLAYER_2;
                if (sectionName && strcmp(sectionName, "Mahjong") == 0 && IsMahjongP2(action))
                    player = PLAYER_2;

                char* dup = _strdup(pair.value.c_str());
                char* tok = strtok(dup, ", ");
                while (tok) { ParseSdlSource(tok, player, action); tok = strtok(nullptr, ", "); }
                free(dup);
            }
            return true;
        }
    }
    return common != nullptr;
}
