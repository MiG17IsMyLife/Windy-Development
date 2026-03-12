/**
 * @file defaultbindings.cpp
 * @brief Default control binding tables for each game type.
 * C++ port of lindbergh-loader's controlIniGen.c
 */
#include "sdlinput.h"
#include <SDL3/SDL.h>

// ============================================================
// Action Name Map (for INI parsing)
// ============================================================
const ActionNameEntry g_actionNameMap[] = {
    {"Test", LA_Test},
    {"Coin", LA_Coin},
    {"Start", LA_Start},
    {"Service", LA_Service},
    {"Up", LA_Up},
    {"Down", LA_Down},
    {"Left", LA_Left},
    {"Right", LA_Right},
    {"Button1", LA_Button1},
    {"Button2", LA_Button2},
    {"Button3", LA_Button3},
    {"Button4", LA_Button4},
    {"Button5", LA_Button5},
    {"Button6", LA_Button6},
    {"Button7", LA_Button7},
    {"Button8", LA_Button8},
    {"Button9", LA_Button9},
    {"Button10", LA_Button10},
    {"Trigger", LA_Trigger},
    {"OutOfScreen", LA_OutOfScreen},
    {"Reload", LA_Reload},
    {"GunButton", LA_GunButton},
    {"ActionButton", LA_ActionButton},
    {"ChangeButton", LA_ChangeButton},
    {"PedalLeft", LA_PedalLeft},
    {"PedalRight", LA_PedalRight},
    {"GunX", LA_GunX},
    {"GunY", LA_GunY},
    {"GearUp", LA_GearUp},
    {"GearDown", LA_GearDown},
    {"ViewChange", LA_ViewChange},
    {"MusicChange", LA_MusicChange},
    {"Boost", LA_Boost},
    {"BoostRight", LA_BoostRight},
    {"Steer", LA_Steer},
    {"Gas", LA_Gas},
    {"Brake", LA_Brake},
    {"Steer_Left", LA_Steer_Left},
    {"Steer_Right", LA_Steer_Right},
    {"Gas_Digital", LA_Gas_Digital},
    {"Brake_Digital", LA_Brake_Digital},
    {"CardInsert", LA_CardInsert},
    {"Card1Insert", LA_Card1Insert},
    {"Card2Insert", LA_Card2Insert},
    {"ABC_X", LA_ABC_X},
    {"ABC_Left", LA_ABC_Left},
    {"ABC_Right", LA_ABC_Right},
    {"ABC_Y", LA_ABC_Y},
    {"ABC_Up", LA_ABC_Up},
    {"ABC_Down", LA_ABC_Down},
    {"Throttle", LA_Throttle},
    {"Throttle_Accelerate", LA_Throttle_Accelerate},
    {"Throttle_Slowdown", LA_Throttle_Slowdown},
    {"GunTrigger", LA_GunTrigger},
    {"MissileTrigger", LA_MissileTrigger},
    {"ClimaxSwitch", LA_ClimaxSwitch},
    {"ButtonA", LA_A},
    {"ButtonB", LA_B},
    {"ButtonC", LA_C},
    {"ButtonD", LA_D},
    {"ButtonE", LA_E},
    {"ButtonF", LA_F},
    {"ButtonG", LA_G},
    {"ButtonH", LA_H},
    {"ButtonI", LA_I},
    {"ButtonJ", LA_J},
    {"ButtonK", LA_K},
    {"ButtonL", LA_L},
    {"ButtonM", LA_M},
    {"ButtonN", LA_N},
    {"ButtonReach", LA_Reach},
    {"ButtonChi", LA_Chi},
    {"ButtonPon", LA_Pon},
    {"ButtonKan", LA_Kan},
    {"ButtonAgari", LA_Agari},
    {"ButtonCancel", LA_Cancel},
};
const int g_numActionNames = sizeof(g_actionNameMap) / sizeof(g_actionNameMap[0]);

// ============================================================
// Helper macro for readability
// ============================================================
#define KB(scancode, player, action) {INPUT_TYPE_KEY, 0, scancode, AXIS_MODE_DIGITAL, 0, false, player, action}
#define MB(button, player, action) {INPUT_TYPE_MOUSE_BUTTON, 0, button, AXIS_MODE_DIGITAL, 0, false, player, action}
#define MA(axis, player, action) {INPUT_TYPE_MOUSE_AXIS, 0, axis, AXIS_MODE_FULL, 0, false, player, action}
#define JB(dev, btn, player, action) {INPUT_TYPE_JOY_BUTTON, dev, btn, AXIS_MODE_DIGITAL, 0, false, player, action}
#define JA(dev, ax, mode, player, action) {INPUT_TYPE_JOY_AXIS, dev, ax, mode, 0, false, player, action}
#define JA_D(dev, ax, thr, player, action) {INPUT_TYPE_JOY_AXIS, dev, ax, AXIS_MODE_DIGITAL, thr, false, player, action}
#define JH(dev, hat, dir, player, action) {INPUT_TYPE_JOY_HAT, dev, hat, AXIS_MODE_DIGITAL, dir, false, player, action}
#define GB(dev, btn, player, action) {INPUT_TYPE_GAMEPAD_BUTTON, dev, btn, AXIS_MODE_DIGITAL, 0, false, player, action}
#define GA(dev, ax, mode, player, action) {INPUT_TYPE_GAMEPAD_AXIS, dev, ax, mode, 0, false, player, action}
#define GA_D(dev, ax, thr, player, action) {INPUT_TYPE_GAMEPAD_AXIS, dev, ax, AXIS_MODE_DIGITAL, thr, false, player, action}
#define GA_INV(dev, ax, mode, player, action) {INPUT_TYPE_GAMEPAD_AXIS, dev, ax, mode, 0, true, player, action}

// ============================================================
// Common Bindings (all game types)
// ============================================================
const ControlBinding g_defaultCommonBindings[] = {
    KB(SDL_SCANCODE_T, SYSTEM, LA_Test),
    KB(SDL_SCANCODE_5, PLAYER_1, LA_Coin),
    KB(SDL_SCANCODE_6, PLAYER_2, LA_Coin),
    KB(SDL_SCANCODE_1, PLAYER_1, LA_Start),
    KB(SDL_SCANCODE_2, PLAYER_2, LA_Start),
    KB(SDL_SCANCODE_S, PLAYER_1, LA_Service),
    JB(0, 11, PLAYER_1, LA_Start),
    JB(0, 10, PLAYER_1, LA_Service),
    GB(0, SDL_GAMEPAD_BUTTON_START, PLAYER_1, LA_Start),
    GB(0, SDL_GAMEPAD_BUTTON_BACK, PLAYER_1, LA_Service),
};
const size_t g_defaultCommonBindingsSize = sizeof(g_defaultCommonBindings) / sizeof(g_defaultCommonBindings[0]);

// ============================================================
// Digital (VF5, etc.)
// ============================================================
const ControlBinding g_defaultDigitalBindings[] = {
    // Keyboard
    KB(SDL_SCANCODE_UP, PLAYER_1, LA_Up),
    KB(SDL_SCANCODE_DOWN, PLAYER_1, LA_Down),
    KB(SDL_SCANCODE_LEFT, PLAYER_1, LA_Left),
    KB(SDL_SCANCODE_RIGHT, PLAYER_1, LA_Right),
    KB(SDL_SCANCODE_Q, PLAYER_1, LA_Button1),
    KB(SDL_SCANCODE_W, PLAYER_1, LA_Button2),
    KB(SDL_SCANCODE_E, PLAYER_1, LA_Button3),
    KB(SDL_SCANCODE_F7, PLAYER_1, LA_Card1Insert),
    KB(SDL_SCANCODE_F8, PLAYER_2, LA_Card2Insert),
    // Gamepad
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_UP, PLAYER_1, LA_Up),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_DOWN, PLAYER_1, LA_Down),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_LEFT, PLAYER_1, LA_Left),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PLAYER_1, LA_Right),
    GB(0, SDL_GAMEPAD_BUTTON_SOUTH, PLAYER_1, LA_Button1),
    GB(0, SDL_GAMEPAD_BUTTON_EAST, PLAYER_1, LA_Button2),
    GB(0, SDL_GAMEPAD_BUTTON_WEST, PLAYER_1, LA_Button3),
    GA_D(0, SDL_GAMEPAD_AXIS_LEFTY, -1, PLAYER_1, LA_Up),
    GA_D(0, SDL_GAMEPAD_AXIS_LEFTY, 1, PLAYER_1, LA_Down),
    GA_D(0, SDL_GAMEPAD_AXIS_LEFTX, 1, PLAYER_1, LA_Right),
    GA_D(0, SDL_GAMEPAD_AXIS_LEFTX, -1, PLAYER_1, LA_Left),
    GB(0, SDL_GAMEPAD_BUTTON_GUIDE, PLAYER_1, LA_Card1Insert),
    GB(1, SDL_GAMEPAD_BUTTON_GUIDE, PLAYER_2, LA_Card2Insert),
    // Joystick
    JH(0, 0, SDL_HAT_UP, PLAYER_1, LA_Up),
    JH(0, 0, SDL_HAT_DOWN, PLAYER_1, LA_Down),
    JH(0, 0, SDL_HAT_LEFT, PLAYER_1, LA_Left),
    JH(0, 0, SDL_HAT_RIGHT, PLAYER_1, LA_Right),
    JB(0, 0, PLAYER_1, LA_Button1),
    JB(0, 1, PLAYER_1, LA_Button2),
    JB(0, 2, PLAYER_1, LA_Button3),
    JA_D(0, 1, -1, PLAYER_1, LA_Up),
    JA_D(0, 1, 1, PLAYER_1, LA_Down),
    JA_D(0, 0, -1, PLAYER_1, LA_Left),
    JA_D(0, 0, 1, PLAYER_1, LA_Right),
};
const size_t g_defaultDigitalBindingsSize = sizeof(g_defaultDigitalBindings) / sizeof(g_defaultDigitalBindings[0]);

// ============================================================
// Driving (OutRun, Hummer, Initial D, etc.)
// ============================================================
const ControlBinding g_defaultDrivingBindings[] = {
    // Keyboard
    KB(SDL_SCANCODE_LEFT, PLAYER_1, LA_Steer_Left),
    KB(SDL_SCANCODE_RIGHT, PLAYER_1, LA_Steer_Right),
    KB(SDL_SCANCODE_UP, PLAYER_1, LA_Gas_Digital),
    KB(SDL_SCANCODE_DOWN, PLAYER_1, LA_Brake_Digital),
    KB(SDL_SCANCODE_V, PLAYER_1, LA_ViewChange),
    KB(SDL_SCANCODE_Q, PLAYER_2, LA_Boost),
    KB(SDL_SCANCODE_W, PLAYER_1, LA_BoostRight),
    KB(SDL_SCANCODE_A, PLAYER_2, LA_GearUp),
    KB(SDL_SCANCODE_Z, PLAYER_2, LA_GearDown),
    KB(SDL_SCANCODE_M, PLAYER_1, LA_MusicChange),
    KB(SDL_SCANCODE_I, PLAYER_1, LA_Up),
    KB(SDL_SCANCODE_K, PLAYER_1, LA_Down),
    KB(SDL_SCANCODE_J, PLAYER_1, LA_Left),
    KB(SDL_SCANCODE_L, PLAYER_1, LA_Right),
    KB(SDL_SCANCODE_F7, PLAYER_1, LA_CardInsert),
    // Gamepad
    GA(0, SDL_GAMEPAD_AXIS_LEFTX, AXIS_MODE_FULL, PLAYER_1, LA_Steer),
    GA(0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_MODE_FULL, PLAYER_1, LA_Gas),
    GA(0, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_MODE_FULL, PLAYER_1, LA_Brake),
    GB(0, SDL_GAMEPAD_BUTTON_SOUTH, PLAYER_2, LA_Boost),
    GB(0, SDL_GAMEPAD_BUTTON_EAST, PLAYER_1, LA_BoostRight),
    GB(0, SDL_GAMEPAD_BUTTON_WEST, PLAYER_1, LA_ViewChange),
    GB(0, SDL_GAMEPAD_BUTTON_NORTH, PLAYER_1, LA_MusicChange),
    GB(0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PLAYER_2, LA_GearUp),
    GB(0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, PLAYER_2, LA_GearDown),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_UP, PLAYER_1, LA_Up),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_DOWN, PLAYER_1, LA_Down),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_LEFT, PLAYER_1, LA_Left),
    GB(0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PLAYER_1, LA_Right),
    GB(0, SDL_GAMEPAD_BUTTON_GUIDE, PLAYER_1, LA_CardInsert),
    // Joystick
    JA(0, 0, AXIS_MODE_FULL, PLAYER_1, LA_Steer),
    JA(0, 5, AXIS_MODE_FULL, PLAYER_1, LA_Gas),
    JA(0, 2, AXIS_MODE_FULL, PLAYER_1, LA_Brake),
    JB(0, 0, PLAYER_2, LA_Boost),
    JB(0, 1, PLAYER_1, LA_BoostRight),
    JB(0, 2, PLAYER_1, LA_ViewChange),
    JB(0, 5, PLAYER_2, LA_GearUp),
    JB(0, 4, PLAYER_2, LA_GearDown),
    JB(0, 3, PLAYER_1, LA_MusicChange),
    JH(0, 0, SDL_HAT_UP, PLAYER_1, LA_Up),
    JH(0, 0, SDL_HAT_DOWN, PLAYER_1, LA_Down),
    JH(0, 0, SDL_HAT_LEFT, PLAYER_1, LA_Left),
    JH(0, 0, SDL_HAT_RIGHT, PLAYER_1, LA_Right),
};
const size_t g_defaultDrivingBindingsSize = sizeof(g_defaultDrivingBindings) / sizeof(g_defaultDrivingBindings[0]);

// ============================================================
// After Burner Climax
// ============================================================
const ControlBinding g_defaultAbcBindings[] = {
    // Keyboard
    KB(SDL_SCANCODE_LEFT, PLAYER_1, LA_ABC_Left),
    KB(SDL_SCANCODE_RIGHT, PLAYER_1, LA_ABC_Right),
    KB(SDL_SCANCODE_UP, PLAYER_1, LA_ABC_Up),
    KB(SDL_SCANCODE_DOWN, PLAYER_1, LA_ABC_Down),
    KB(SDL_SCANCODE_A, PLAYER_1, LA_Throttle_Accelerate),
    KB(SDL_SCANCODE_Z, PLAYER_1, LA_Throttle_Slowdown),
    KB(SDL_SCANCODE_Q, PLAYER_1, LA_GunTrigger),
    KB(SDL_SCANCODE_W, PLAYER_1, LA_MissileTrigger),
    KB(SDL_SCANCODE_E, PLAYER_1, LA_ClimaxSwitch),
    // Gamepad
    GA(0, SDL_GAMEPAD_AXIS_LEFTX, AXIS_MODE_FULL, PLAYER_1, LA_ABC_X),
    GA(0, SDL_GAMEPAD_AXIS_LEFTY, AXIS_MODE_FULL, PLAYER_1, LA_ABC_Y),
    GA_INV(0, SDL_GAMEPAD_AXIS_RIGHTY, AXIS_MODE_FULL, PLAYER_1, LA_Throttle),
    GA(0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_MODE_POSITIVE_HALF, PLAYER_1, LA_Throttle),
    GA(0, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_MODE_NEGATIVE_HALF, PLAYER_1, LA_Throttle),
    GB(0, SDL_GAMEPAD_BUTTON_SOUTH, PLAYER_1, LA_GunTrigger),
    GB(0, SDL_GAMEPAD_BUTTON_EAST, PLAYER_1, LA_MissileTrigger),
    GB(0, SDL_GAMEPAD_BUTTON_NORTH, PLAYER_1, LA_ClimaxSwitch),
    // Joystick
    JA(0, 0, AXIS_MODE_FULL, PLAYER_1, LA_ABC_X),
    JA(0, 1, AXIS_MODE_FULL, PLAYER_1, LA_ABC_Y),
    {INPUT_TYPE_JOY_AXIS, 0, 4, AXIS_MODE_FULL, 0, true, PLAYER_1, LA_Throttle},
    JA(0, 5, AXIS_MODE_POSITIVE_HALF, PLAYER_1, LA_Throttle),
    JA(0, 4, AXIS_MODE_NEGATIVE_HALF, PLAYER_1, LA_Throttle),
    JB(0, 0, PLAYER_1, LA_GunTrigger),
    JB(0, 1, PLAYER_1, LA_MissileTrigger),
    JB(0, 2, PLAYER_1, LA_ClimaxSwitch),
};
const size_t g_defaultAbcBindingsSize = sizeof(g_defaultAbcBindings) / sizeof(g_defaultAbcBindings[0]);

// ============================================================
// Shooting (HOD4, Rambo, LGJ, Ghost Squad, etc.)
// ============================================================
const ControlBinding g_defaultShootingBindings[] = {
    // Mouse / Gun
    MA(0, PLAYER_1, LA_GunX),
    MA(1, PLAYER_1, LA_GunY),
    MB(SDL_BUTTON_LEFT, PLAYER_1, LA_Trigger),
    MB(SDL_BUTTON_RIGHT, PLAYER_1, LA_Reload),
    MB(SDL_BUTTON_MIDDLE, PLAYER_1, LA_GunButton),
    // Keyboard
    KB(SDL_SCANCODE_Q, PLAYER_1, LA_Trigger),
    KB(SDL_SCANCODE_W, PLAYER_1, LA_Reload),
    KB(SDL_SCANCODE_E, PLAYER_1, LA_GunButton),
    KB(SDL_SCANCODE_R, PLAYER_1, LA_ActionButton),
    KB(SDL_SCANCODE_LEFT, PLAYER_1, LA_PedalLeft),
    KB(SDL_SCANCODE_RIGHT, PLAYER_1, LA_PedalRight),
};
const size_t g_defaultShootingBindingsSize = sizeof(g_defaultShootingBindings) / sizeof(g_defaultShootingBindings[0]);

// ============================================================
// Mahjong (MJ4)
// ============================================================
const ControlBinding g_defaultMahjongBindings[] = {
    KB(SDL_SCANCODE_Y, PLAYER_1, LA_A),      KB(SDL_SCANCODE_U, PLAYER_1, LA_B),       KB(SDL_SCANCODE_I, PLAYER_1, LA_C),
    KB(SDL_SCANCODE_O, PLAYER_1, LA_D),      KB(SDL_SCANCODE_G, PLAYER_1, LA_E),       KB(SDL_SCANCODE_H, PLAYER_1, LA_F),
    KB(SDL_SCANCODE_J, PLAYER_1, LA_G),      KB(SDL_SCANCODE_K, PLAYER_1, LA_H),       KB(SDL_SCANCODE_L, PLAYER_1, LA_I),
    KB(SDL_SCANCODE_V, PLAYER_1, LA_J),      KB(SDL_SCANCODE_B, PLAYER_1, LA_K),       KB(SDL_SCANCODE_N, PLAYER_2, LA_L),
    KB(SDL_SCANCODE_M, PLAYER_2, LA_M),      KB(SDL_SCANCODE_COMMA, PLAYER_2, LA_N),   KB(SDL_SCANCODE_F1, PLAYER_2, LA_Chi),
    KB(SDL_SCANCODE_F2, PLAYER_2, LA_Pon),   KB(SDL_SCANCODE_F3, PLAYER_2, LA_Kan),    KB(SDL_SCANCODE_F4, PLAYER_2, LA_Reach),
    KB(SDL_SCANCODE_F5, PLAYER_2, LA_Agari), KB(SDL_SCANCODE_F6, PLAYER_2, LA_Cancel), KB(SDL_SCANCODE_F7, PLAYER_2, LA_CardInsert),
};
const size_t g_defaultMahjongBindingsSize = sizeof(g_defaultMahjongBindings) / sizeof(g_defaultMahjongBindings[0]);