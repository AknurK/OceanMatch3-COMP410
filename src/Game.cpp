#include "Game.h"
#include <algorithm>
#include <ctime>
#include <iostream>
#include <random>
#include <vector>

Game *Game::s_instance = nullptr;

void Game::mouseButtonCB(GLFWwindow *w, int btn, int action, int mods) {
  if (!s_instance)
    return;
  double mx, my;
  glfwGetCursorPos(w, &mx, &my);

  int winW, winH;
  glfwGetWindowSize(w, &winW, &winH);

  // The game content occupies a fixed REFERENCE_SIZE × REFERENCE_SIZE area
  // centred in the window — no scaling, just an offset
  double x_offset = (winW - REFERENCE_SIZE) / 2.0;
  double y_offset = (winH - REFERENCE_SIZE) / 2.0;

  // Convert window coords to the logical space (1:1 mapping, no scaling)
  double scaled_mx = mx - x_offset;
  double scaled_my = my - y_offset;

  // Use ray-plane intersection to find which board cell was clicked
  auto [row, col] = s_instance->renderer.screenToBoard(scaled_mx, scaled_my);
  s_instance->inputMgr.onMouseButton(btn, action, row, col);
}

void Game::framebufferCB(GLFWwindow *w, int fbWidth, int fbHeight) {
  if (!s_instance)
    return;
  // Just store the new framebuffer dimensions; viewport is applied per-frame
  s_instance->cachedFBWidth = fbWidth;
  s_instance->cachedFBHeight = fbHeight;

  int winW, winH;
  glfwGetWindowSize(w, &winW, &winH);
  s_instance->cachedPixelScale =
      (winW > 0) ? (float)fbWidth / (float)winW : 1.0f;
}

void Game::updateBackgroundCameraControls(float dt) {
  if (!window)
    return;

  double mx, my;
  glfwGetCursorPos(window, &mx, &my);

  float yawDeltaDeg = 0.0f;
  const bool rightMouseDown =
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  if (rightMouseDown) {
    if (backgroundLookActive) {
      yawDeltaDeg = static_cast<float>(mx - lastBackgroundMouseX) * 0.045f;
    } else {
      backgroundLookActive = true;
    }
  } else {
    backgroundLookActive = false;
  }

  lastBackgroundMouseX = mx;

  auto pressed = [this](int key) {
    return glfwGetKey(window, key) == GLFW_PRESS;
  };

  float forward = 0.0f;
  float strafe = 0.0f;
  if (pressed(GLFW_KEY_W))
    forward += 1.0f;
  if (pressed(GLFW_KEY_S))
    forward -= 1.0f;
  if (pressed(GLFW_KEY_D))
    strafe += 1.0f;
  if (pressed(GLFW_KEY_A))
    strafe -= 1.0f;

  constexpr float forwardSpeed = 0.9f;
  constexpr float strafeSpeed = 0.55f;
  oceanBackground.updateCameraControls(
      yawDeltaDeg, forward * forwardSpeed * dt, strafe * strafeSpeed * dt,
      pressed(GLFW_KEY_R));
}

bool Game::init() {
  s_instance = this;

  if (!glfwInit()) {
    std::cerr << "GLFW init failed\n";
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(REFERENCE_SIZE, REFERENCE_SIZE, "OceanMatch3",
                            nullptr, nullptr);
  if (!window) {
    std::cerr << "Window creation failed\n";
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync

  // Enable depth testing for 3D rendering
  glEnable(GL_DEPTH_TEST);

  // Register callbacks
  glfwSetMouseButtonCallback(window, mouseButtonCB);
  glfwSetFramebufferSizeCallback(window, framebufferCB);

  // Initialise cached framebuffer dimensions so the very first frame is correct
  {
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    cachedFBWidth = fbW;
    cachedFBHeight = fbH;
    int wW, wH;
    glfwGetWindowSize(window, &wW, &wH);
    cachedPixelScale = (wW > 0) ? (float)fbW / (float)wW : 1.0f;
  }

  // Load shaders  (paths relative to working directory = project root)
  shader = Shader("shaders/tile.vert", "shaders/tile.frag");
  cubeShader = Shader("shaders/cube.vert", "shaders/cube.frag");
  renderer.init(REFERENCE_SIZE, REFERENCE_SIZE, &shader, &cubeShader);
  oceanBackground.init();

  // Input
  inputMgr.init();
  inputMgr.setSwapCallback(
      [this](int r1, int c1, int r2, int c2) { onSwap(r1, c1, r2, c2); });

  // Build board
  board = Board(BOARD_ROWS, BOARD_COLS);

  // Randomly place 6–12 obstacles at unique positions
  {
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

    // Random obstacle count between 6 and 12
    std::uniform_int_distribution<int> countDist(6, 12);
    int obstacleCount = countDist(rng);

    // Build list of all valid cell positions and shuffle
    std::vector<std::pair<int, int>> positions;
    positions.reserve(BOARD_ROWS * BOARD_COLS);
    for (int r = 0; r < BOARD_ROWS; r++)
      for (int c = 0; c < BOARD_COLS; c++)
        positions.push_back({r, c});
    std::shuffle(positions.begin(), positions.end(), rng);

    // Place obstacles at the first N shuffled positions
    std::uniform_int_distribution<int> typeDist(0, 1); // 0 = ICE, 1 = WOOD
    for (int i = 0; i < obstacleCount && i < (int)positions.size(); i++) {
      auto [r, c] = positions[i];
      TileType obsType = (typeDist(rng) == 0) ? TileType::OBSTACLE_ICE
                                              : TileType::OBSTACLE_WOOD;
      board.placeObstacle(r, c, obsType, 0);
    }
  }
  board.randomFill();

  // Squash any initial matches silently
  while (board.resolveMatches()) {
    board.applyGravity();
    board.refill();
  }

  return true;
}

void Game::onSwap(int r1, int c1, int r2, int c2) {
  if (swapState != SwapState::IDLE)
    return;

  // Basic validation — reject silently (no animation)
  if (r1 < 0 || r1 >= board.rows || c1 < 0 || c1 >= board.cols)
    return;
  if (r2 < 0 || r2 >= board.rows || c2 < 0 || c2 >= board.cols)
    return;
  if (std::abs(r2 - r1) + std::abs(c2 - c1) != 1)
    return;
  if (board.grid[r1][c1].isObstacle() || board.grid[r2][c2].isObstacle())
    return;
  if (board.grid[r1][c1].type == TileType::NONE ||
      board.grid[r2][c2].type == TileType::NONE)
    return;

  // Dry-run: test whether the swap would be valid.
  // A swap is valid if either tile is special (triggers its effect)
  // OR the swap produces at least one match.
  if (board.grid[r1][c1].hasSpecial() || board.grid[r2][c2].hasSpecial()) {
    animSwapValid = true;
  } else {
    std::swap(board.grid[r1][c1], board.grid[r2][c2]);
    animSwapValid = !board.findMatches().empty();
    std::swap(board.grid[r1][c1], board.grid[r2][c2]); // always revert
  }

  // Start forward animation
  animR1 = r1;
  animC1 = c1;
  animR2 = r2;
  animC2 = c2;
  animProgress = 0.0f;
  swapState = SwapState::FORWARD;
  inputMgr.inputBlocked = true;
}

static bool boardHasEmptyCells(const Board &board) {
  for (int r = 0; r < board.rows; r++) {
    for (int c = 0; c < board.cols; c++) {
      if (board.grid[r][c].type == TileType::NONE) {
        return true;
      }
    }
  }
  return false;
}

void Game::updateAnimation(float dt) {
  if (swapState == SwapState::IDLE)
    return;

  // ── Swap forward ────────────────────────────────────────────────────
  if (swapState == SwapState::FORWARD) {
    animProgress += dt * SWAP_ANIM_SPEED;
    if (animProgress < 1.0f)
      return;
    animProgress = 1.0f;
    if (!animSwapValid) {
      swapState = SwapState::BACK;
      return;
    }
    // Valid swap → commit and enter resolve cascade
    board.swapTiles(animR1, animC1, animR2, animC2);
    swapState = SwapState::RESOLVING;
    // fall through to RESOLVING
  }

  // ── Swap back ───────────────────────────────────────────────────────
  if (swapState == SwapState::BACK) {
    animProgress -= dt * SWAP_ANIM_SPEED;
    if (animProgress <= 0.0f) {
      animProgress = 0.0f;
      swapState = SwapState::IDLE;
      inputMgr.inputBlocked = false;
    }
    return;
  }

  // ── Special tile explosion animation ───────────────────────────────────
  // When a special tile is triggered, the board cells are already cleared
  // logically, but gravity/refill waits until this visual effect finishes.
  if (swapState == SwapState::EXPLODING) {
    explosionProgress += dt * EXPLOSION_ANIM_SPEED;

    if (explosionProgress < 1.0f) {
      return;
    }

    explosionProgress = 1.0f;

    // After the explosion effect is complete, start gravity/refill.
    auto preGravity = board.grid;

    board.applyGravity();
    board.refill();
    computeFallOffsets(preGravity);

    board.clearLastSpecialAffectedCells();

    explosionProgress = 0.0f;
    swapState = SwapState::FALLING;
    return;
  }

  // ── Falling ─────────────────────────────────────────────────────────
  if (swapState == SwapState::FALLING) {
    bool allDone = true;
    for (int r = 0; r < BOARD_ROWS; r++)
      for (int c = 0; c < BOARD_COLS; c++)
        if (fallOffsets[r][c] < 0.0f) {
          fallOffsets[r][c] += FALL_SPEED * dt;
          if (fallOffsets[r][c] >= 0.0f)
            fallOffsets[r][c] = 0.0f;
          else
            allDone = false;
        }
    if (!allDone)
      return;
    swapState = SwapState::RESOLVING;
    // fall through to RESOLVING to check for cascading matches
  }

  // ── Resolving (instant logic step) ──────────────────────────────────
  if (swapState == SwapState::RESOLVING) {
    bool resolvedMatches = board.resolveMatches();
    bool specialTriggered = !board.getLastSpecialAffectedCells().empty();
    bool hasHoles = boardHasEmptyCells(board);

    // If a special tile was triggered, pause the cascade and play the
    // custom explosion animation first. Gravity/refill will happen after
    // EXPLODING finishes.
    if (specialTriggered) {
      explosionProgress = 0.0f;
      swapState = SwapState::EXPLODING;
      return;
    }

    if (resolvedMatches || hasHoles) {
      // Save state after all clears, before gravity
      auto preGravity = board.grid;

      board.applyGravity();
      board.refill();
      computeFallOffsets(preGravity);

      board.clearLastSpecialAffectedCells();
      swapState = SwapState::FALLING;
    } else {
      // No more matches and no pending cleared cells
      swapState = SwapState::IDLE;
      inputMgr.inputBlocked = false;
    }
  }
}

void Game::computeFallOffsets(
    const std::vector<std::vector<Cell>> &preGravity) {
  // Zero all offsets
  for (int r = 0; r < BOARD_ROWS; r++)
    for (int c = 0; c < BOARD_COLS; c++)
      fallOffsets[r][c] = 0.0f;

  // Process each column, segment by segment (segments bounded by obstacles)
  for (int c = 0; c < BOARD_COLS; c++) {
    int segStart = -1;
    for (int r = 0; r <= BOARD_ROWS; r++) {
      bool isObs = (r < BOARD_ROWS && board.grid[r][c].isObstacle());
      if (segStart < 0) {
        if (r < BOARD_ROWS && !isObs)
          segStart = r;
      } else {
        if (isObs || r == BOARD_ROWS) {
          int segEnd = r - 1;

          // Collect source rows of surviving tiles (pre-gravity)
          std::vector<int> srcRows;
          for (int sr = segStart; sr <= segEnd; sr++) {
            if (preGravity[sr][c].type != TileType::NONE &&
                !preGravity[sr][c].isObstacle()) {
              srcRows.push_back(sr);
            }
          }

          int N = (int)srcRows.size();
          int segSize = segEnd - segStart + 1;
          int M = segSize - N; // number of refill tiles

          // Surviving tiles pack to bottom of the segment after gravity
          for (int i = 0; i < N; i++) {
            int destRow = segEnd - N + 1 + i;
            fallOffsets[destRow][c] = (float)(srcRows[i] - destRow);
          }

          // Refill tiles enter from above the segment
          for (int i = 0; i < M; i++) {
            int destRow = segStart + i;
            fallOffsets[destRow][c] = (float)(-M);
          }

          segStart = -1;
        }
      }
    }
  }
}

void Game::run() {
  float lastTime = static_cast<float>(glfwGetTime());

  while (!glfwWindowShouldClose(window)) {
    float now = static_cast<float>(glfwGetTime());
    float dt = now - lastTime;
    lastTime = now;

    glfwPollEvents();
    updateBackgroundCameraControls(dt);

    // Update animation state machine
    updateAnimation(dt);

    // Update rotation angle — slow idle animation (~0.3 rad/sec)
    float rotAngle = now * 0.3f;
    renderer.setRotationAngle(rotAngle);

    // Build swap animation data (only during FORWARD/BACK)
    SwapAnim swapAnim;
    if (swapState == SwapState::FORWARD || swapState == SwapState::BACK) {
      swapAnim.active = true;
      swapAnim.r1 = animR1;
      swapAnim.c1 = animC1;
      swapAnim.r2 = animR2;
      swapAnim.c2 = animC2;
      swapAnim.progress = animProgress;
    }

    // Build fall animation data (only during FALLING)
    FallAnim fallAnim;
    if (swapState == SwapState::FALLING) {
      fallAnim.active = true;
      for (int r = 0; r < BOARD_ROWS; r++)
        for (int c = 0; c < BOARD_COLS; c++)
          fallAnim.offsets[r][c] = fallOffsets[r][c];
    }
        // Build special explosion animation data
    ExplosionAnim explosionAnim;
    if (swapState == SwapState::EXPLODING) {
      explosionAnim.active = true;
      explosionAnim.type = board.getLastSpecialType();
      auto [originR, originC] = board.getLastSpecialOrigin();
      explosionAnim.originR = originR;
      explosionAnim.originC = originC;
      explosionAnim.progress = explosionProgress;
      explosionAnim.affectedCells = board.getLastSpecialAffectedCells();
    }

    // ── Set viewport every frame ────────────────────────────────────────
    // 1) Clear the FULL framebuffer to the background colour so that areas
    //    outside the 900×900 content region show a solid ocean-blue fill.
    glViewport(0, 0, cachedFBWidth, cachedFBHeight);
    renderer.beginFrame();   // glClear with background colour
    oceanBackground.render(now, cachedFBWidth, cachedFBHeight);

    // 2) Set the fixed-size viewport: exactly REFERENCE_SIZE logical pixels
    //    centred within the framebuffer.  Content is NEVER scaled — it is
    //    rendered 1:1.  On smaller windows the edges are cropped; on larger
    //    windows the surplus is background.
    int vpSize = (int)((float)REFERENCE_SIZE * cachedPixelScale);
    int vpX = (cachedFBWidth - vpSize) / 2;
    int vpY = (cachedFBHeight - vpSize) / 2;
    glViewport(vpX, vpY, vpSize, vpSize);

    // The background uses several transparent and additive passes. Restore the
    // baseline state expected by the board renderer before drawing the game.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    renderer.drawBoard(board, inputMgr.selRow, inputMgr.selCol, swapAnim,
                       fallAnim, explosionAnim);
    glfwSwapBuffers(window);
  }
  glfwDestroyWindow(window);
  glfwTerminate();
}
