#pragma once

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "Board.h"
#include "Renderer.h"
#include "InputManager.h"
#include "Shader.h"
#include "background/OceanBackgroundRenderer.h"

// The canonical logical size that all content is authored for.
// Content is always rendered at exactly this size (1:1 pixels).
constexpr int REFERENCE_SIZE = 900;

class Game {
public:
    bool init();
    void run();

private:
    GLFWwindow*  window   = nullptr;
    Board        board;
    Renderer     renderer;
    OceanBackgroundRenderer oceanBackground;
    InputManager inputMgr;
    Shader       shader;       // 2D tile shader
    Shader       cubeShader;   // 3D cube shader

    void onSwap(int r1, int c1, int r2, int c2);

    // ── Animation state machine ────────────────────────────────────────────
    enum class SwapState { IDLE, FORWARD, BACK, RESOLVING, EXPLODING, FALLING };
    SwapState swapState = SwapState::IDLE;
    int   animR1 = 0, animC1 = 0, animR2 = 0, animC2 = 0;
    float animProgress  = 0.0f;
    bool  animSwapValid = false;
    static constexpr float SWAP_ANIM_SPEED = 4.0f; // 1/speed = 0.25s per direction

    // ── Special tile explosion animation ───────────────────────────────────
    // Used after a special tile is triggered, before gravity/refill starts.
    float explosionProgress = 0.0f;
    static constexpr float EXPLOSION_ANIM_SPEED = 2.8f;
    
    // ── Gravity / fall animation ─────────────────────────────────────────
    float fallOffsets[BOARD_ROWS][BOARD_COLS] = {};
    static constexpr float FALL_SPEED = 10.0f; // rows per second

    void updateBackgroundCameraControls(float dt);
    void updateAnimation(float dt);
    void computeFallOffsets(const std::vector<std::vector<Cell>>& preGravity);

    // ── Cached framebuffer state (updated by callback, used per-frame) ──────
    int   cachedFBWidth  = 0;
    int   cachedFBHeight = 0;
    float cachedPixelScale = 1.0f; // framebuffer-pixels / logical-pixels
    bool  backgroundLookActive = false;
    double lastBackgroundMouseX = 0.0;

    // GLFW static callbacks
    static Game* s_instance;
    static void mouseButtonCB(GLFWwindow* w, int btn, int action, int mods);
    static void framebufferCB(GLFWwindow* w, int width, int height);
};
