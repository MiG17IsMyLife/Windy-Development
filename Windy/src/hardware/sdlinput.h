/**
 * @file sdlinput.h
 * @brief SDL Input System - C++ port of lindbergh-loader's sdlInput
 *
 * Three-layer input architecture:
 *   Layer 1: Physical Input (SDL events) → Binding Tables
 *   Layer 2: Logical Actions (game-agnostic)
 *   Layer 3: JVS Mapping (game-type specific)
 */
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <mutex>
#include <cstdint>
#include "jvsboard.h"

// ============================================================
// Constants
// ============================================================
#define MAX_PLAYERS         4
#define MAX_ENTITIES        (MAX_PLAYERS + 1)  // Players + SYSTEM
#define MAX_JOYSTICKS       8
#define MAX_JOY_BUTTONS     32
#define MAX_JOY_AXES        32
#define MAX_JOY_HATS        4
#define MAX_MOUSE_BUTTONS   16

// ============================================================
// Logical Actions (Layer 2)
// ============================================================
enum LogicalAction {
    LA_INVALID = -1,

    // Common
    LA_Test,
    LA_Coin,
    LA_Start,
    LA_Service,

    // Digital (fighting/VF5 etc.)
    LA_Up,
    LA_Down,
    LA_Left,
    LA_Right,
    LA_Button1,
    LA_Button2,
    LA_Button3,
    LA_Button4,
    LA_Button5,
    LA_Button6,
    LA_Button7,
    LA_Button8,
    LA_Button9,
    LA_Button10,

    // Shooting
    LA_Trigger,
    LA_OutOfScreen,
    LA_Reload,
    LA_GunButton,
    LA_ActionButton,
    LA_ChangeButton,
    LA_PedalLeft,
    LA_PedalRight,
    LA_GunX,
    LA_GunY,

    // Driving
    LA_GearUp,
    LA_GearDown,
    LA_ViewChange,
    LA_MusicChange,
    LA_Boost,
    LA_BoostRight,
    LA_Steer,
    LA_Gas,
    LA_Brake,
    LA_Steer_Left,
    LA_Steer_Right,
    LA_Gas_Digital,
    LA_Brake_Digital,
    LA_CardInsert,
    LA_Card1Insert,
    LA_Card2Insert,

    // After Burner Climax
    LA_ABC_X,
    LA_ABC_Left,
    LA_ABC_Right,
    LA_ABC_Y,
    LA_ABC_Up,
    LA_ABC_Down,
    LA_Throttle,
    LA_Throttle_Accelerate,
    LA_Throttle_Slowdown,
    LA_GunTrigger,
    LA_MissileTrigger,
    LA_ClimaxSwitch,

    // Mahjong
    LA_A, LA_B, LA_C, LA_D, LA_E, LA_F, LA_G,
    LA_H, LA_I, LA_J, LA_K, LA_L, LA_M, LA_N,
    LA_Reach, LA_Chi, LA_Pon, LA_Kan, LA_Agari, LA_Cancel,

    NUM_LOGICAL_ACTIONS
};

// ============================================================
// Input Types
// ============================================================
enum SDLInputType {
    INPUT_TYPE_NONE = 0,
    INPUT_TYPE_KEY,
    INPUT_TYPE_JOY_AXIS,
    INPUT_TYPE_JOY_BUTTON,
    INPUT_TYPE_JOY_HAT,
    INPUT_TYPE_MOUSE_AXIS,
    INPUT_TYPE_MOUSE_BUTTON,
    INPUT_TYPE_GAMEPAD_AXIS,
    INPUT_TYPE_GAMEPAD_BUTTON
};

enum AxisMode {
    AXIS_MODE_FULL,
    AXIS_MODE_POSITIVE_HALF,
    AXIS_MODE_NEGATIVE_HALF,
    AXIS_MODE_DIGITAL
};

// ============================================================
// Binding Structures
// ============================================================
struct ControlBinding {
    SDLInputType type = INPUT_TYPE_NONE;
    int          deviceIndex = 0;
    int          sdlId = 0;
    AxisMode     axisMode = AXIS_MODE_DIGITAL;
    int          axisThreshold = 0;
    bool         isInverted = false;
    JVSPlayer    player = PLAYER_1;
    LogicalAction action = LA_INVALID;
};

struct BindingPair {
    ControlBinding bindings[2];
};

struct HatBinding {
    ControlBinding up;
    ControlBinding down;
    ControlBinding left;
    ControlBinding right;
};

// ============================================================
// State Structures
// ============================================================
struct ActionState {
    bool  isActive = false;
    float analogValue = 0.0f;
    float positiveContribution = 0.0f;
    float negativeContribution = 0.0f;
};

struct JVSActionMapping {
    enum CallType { JVS_CALL_SWITCH, JVS_CALL_ANALOGUE, JVS_CALL_COIN, JVS_CALL_NONE };
    CallType call_type = JVS_CALL_NONE;
    JVSInput jvsInput  = NONE;
};

struct ChangedAction {
    JVSPlayer     player;
    LogicalAction action;
};

struct LogicalActionProperties {
    bool isCentering   = false;
    bool isCombinedAxis = false;
    int  deadzone      = 8000;
};

struct ActionNameEntry {
    const char*   name;
    LogicalAction action;
};

// Viewport destination for mouse coordinate mapping
struct ViewDest {
    int   W = 0, H = 0, X = 0, Y = 0;
    float gameScale = 1.0f;
};

// ============================================================
// SDLControllers
// ============================================================
struct SDLControllers {
    SDL_Joystick* joysticks[MAX_JOYSTICKS]   = {};
    SDL_Gamepad*  controllers[MAX_JOYSTICKS] = {};
    int           joysticksCount = 0;
};

// ============================================================
// SdlInput Class
// ============================================================
class SdlInput {
public:
    static SdlInput& Instance();

    /**
     * @brief Initialize the input system
     * @param controlsPath Path to controls.ini
     * @return 0 on success
     */
    int Init(const char* controlsPath = "controls.ini");

    /**
     * @brief Shut down the input system
     */
    void Shutdown();

    /**
     * @brief Process a single SDL event
     */
    void ProcessEvent(const SDL_Event* e);

    /**
     * @brief Process all changed actions and send to JVS
     * Call this once per frame after all events have been processed.
     */
    void ProcessChangedActions();

    /**
     * @brief Update combined axes (call once per frame)
     */
    void UpdateCombinedAxes();

    /**
     * @brief Update gun shake effect for HOD4 (call once per frame)
     */
    void UpdateGunShake();

    /**
     * @brief Set the viewport destination for mouse coordinate mapping
     */
    void SetViewDest(int x, int y, int w, int h);

    // Accessors
    bool IsInitialized() const { return m_initialized; }
    SDLControllers& GetControllers() { return m_controllers; }

private:
    SdlInput();
    ~SdlInput();
    SdlInput(const SdlInput&) = delete;
    SdlInput& operator=(const SdlInput&) = delete;

    // --- Initialization ---
    void InitJvsMappings();
    void InitActionProperties();
    void SetDefaultMappings();
    void RemapPerGame();
    void DetectCombinedAxes();

    // --- Binding Management ---
    void AddBinding(ControlBinding binding);
    void ParseSdlSource(const char* token, JVSPlayer player, LogicalAction action);

    // --- INI Loading ---
    bool LoadProfileFromIni(const char* controlsPath);
    bool ParseActionKey(const char* key, JVSPlayer* outPlayer, LogicalAction* outAction);
    JVSPlayer FixPlayerForAction(LogicalAction action, int player);
    bool IsMahjongP2(LogicalAction action);

    // --- Event Processing Helpers ---
    void AddActionToDirtyList(JVSPlayer player, LogicalAction action);
    int  GetControllerID(SDL_JoystickID instanceId);
    int  GetJoystickID(SDL_JoystickID instanceId);

    // --- Scan helpers ---
    void ScanAxisBindings(BindingPair* pair, bool hasFullAxis[], bool hasHalfAxis[],
                          bool hasPositive[][NUM_LOGICAL_ACTIONS],
                          bool hasNegative[][NUM_LOGICAL_ACTIONS]);

    // --- State ---
    bool m_initialized = false;

    ActionState            m_actionStates[MAX_ENTITIES][NUM_LOGICAL_ACTIONS];
    JVSActionMapping       m_jvsMap[MAX_ENTITIES][NUM_LOGICAL_ACTIONS];
    LogicalActionProperties m_actionProps[MAX_ENTITIES][NUM_LOGICAL_ACTIONS];
    ChangedAction          m_changedActions[NUM_LOGICAL_ACTIONS * MAX_ENTITIES];
    int                    m_numChangedActions = 0;

    // Binding lookup tables
    ControlBinding m_keyBindings[SDL_SCANCODE_COUNT];
    ControlBinding m_mouseButtonBindings[MAX_MOUSE_BUTTONS];
    ControlBinding m_mouseAxisBindings[2]; // 0=X, 1=Y
    BindingPair    m_joyAxisBindings[MAX_JOYSTICKS][MAX_JOY_AXES];
    ControlBinding m_joyButtonBindings[MAX_JOYSTICKS][MAX_JOY_BUTTONS];
    BindingPair    m_controllerAxisBindings[MAX_JOYSTICKS][SDL_GAMEPAD_AXIS_COUNT];
    ControlBinding m_controllerButtonBindings[MAX_JOYSTICKS][SDL_GAMEPAD_BUTTON_COUNT];
    HatBinding     m_joyHatBindings[MAX_JOYSTICKS][MAX_JOY_HATS];

    SDLControllers m_controllers;
    ViewDest       m_dest;

    int m_jvsAnalogueMaxValue = 1023;
    int m_jvsAnalogueCenterValue = 511;

    // Gun shake state (HOD4)
    int   m_lastGunX[MAX_ENTITIES] = {};
    int   m_lastGunY[MAX_ENTITIES] = {};
    int   m_lastGunXDir[MAX_ENTITIES] = {};
    int   m_lastGunYDir[MAX_ENTITIES] = {};
    float m_shakeValue[MAX_ENTITIES] = {};
    float m_shakeIncreaseRate = 10.0f;
    float m_shakeDecayRate = 0.95f;
};

// ============================================================
// Default Bindings (defined in defaultbindings.cpp)
// ============================================================
extern const ControlBinding g_defaultCommonBindings[];
extern const size_t         g_defaultCommonBindingsSize;
extern const ControlBinding g_defaultDigitalBindings[];
extern const size_t         g_defaultDigitalBindingsSize;
extern const ControlBinding g_defaultDrivingBindings[];
extern const size_t         g_defaultDrivingBindingsSize;
extern const ControlBinding g_defaultAbcBindings[];
extern const size_t         g_defaultAbcBindingsSize;
extern const ControlBinding g_defaultShootingBindings[];
extern const size_t         g_defaultShootingBindingsSize;
extern const ControlBinding g_defaultMahjongBindings[];
extern const size_t         g_defaultMahjongBindingsSize;

// Action name table for INI parsing
extern const ActionNameEntry g_actionNameMap[];
extern const int             g_numActionNames;
