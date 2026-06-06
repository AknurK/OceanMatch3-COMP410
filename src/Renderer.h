#pragma once
#include "Board.h"
#include "Shader.h"
#include <utility>
#include <vector>

struct GLFWwindow;

// Swap animation data passed from Game to Renderer
struct SwapAnim {
  bool active = false;
  int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
  float progress = 0.0f; // 0 = original positions, 1 = swapped positions
};

// Gravity/fall animation data passed from Game to Renderer
struct FallAnim {
  bool active = false;
  float offsets[BOARD_ROWS][BOARD_COLS] =
      {}; // worldZ offset per cell (negative = above)
};

// Special-tile explosion animation data passed from Game to Renderer
struct ExplosionAnim {
  bool active = false;
  SpecialType type = SpecialType::NONE;
  int originR = -1, originC = -1;
  float progress = 0.0f;
  std::vector<std::pair<int, int>> affectedCells;
};

class Renderer {
public:
  // Call once after OpenGL context is current
  void init(int windowW, int windowH, Shader *shader2D, Shader *shader3D);

  void beginFrame();
  void setRotationAngle(float angle);
  void drawBoard(const Board &board, int selRow, int selCol,
                 const SwapAnim &swapAnim = {}, const FallAnim &fallAnim = {},
                 const ExplosionAnim &explosionAnim = {});

  // Ray-plane intersection picking: screen coords (0..winW, 0..winH) -> board
  // (row, col) Returns {-1,-1} if the ray misses the board
  std::pair<int, int> screenToBoard(double screenX, double screenY) const;

private:
  // ── 2D quad pipeline (empty cells) ───────────────────────────────────
  unsigned int vao = 0, vbo = 0, ebo = 0;
  unsigned int tileTextures[4]{};
  unsigned int iceObstacleTex = 0;
  unsigned int woodFullTex = 0;
  unsigned int woodDamagedTex = 0;
  unsigned int piranhaTex = 0;
  unsigned int pufferfishTex = 0;
  Shader *sh = nullptr; // 2D tile shader
  int winW = 900, winH = 900;
  float proj[16]{}; // 2D orthographic projection

  unsigned int loadTexture(const char *path);
  void buildOrtho(float w, float h);
  void drawQuad(float x, float y, float size, float r, float g, float b,
                float a);
  void drawTexturedQuad(unsigned int tex, float x, float y, float size, float r,
                        float g, float b, float a);

  // ── 3D cube pipeline (regular tiles & obstacles) ─────────────────────
  unsigned int cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;

  // Minimal custom mesh for PUFFERFISH
  unsigned int pufferVAO = 0, pufferVBO = 0, pufferEBO = 0;
  int pufferIndexCount = 0;
  // Simple cone mesh for puffer spikes
  unsigned int spikeVAO = 0, spikeVBO = 0, spikeEBO = 0;
  int spikeIndexCount = 0;
  // Flat diamond mesh for pufferfish fins and tail
  unsigned int pufferFinVAO = 0, pufferFinVBO = 0, pufferFinEBO = 0;
  int pufferFinIndexCount = 0;
  // Minimal custom mesh for PIRANHA
  unsigned int piranhaVAO = 0, piranhaVBO = 0, piranhaEBO = 0;
  int piranhaIndexCount = 0;

  Shader *cubeSh = nullptr; // 3D cube shader
  float viewMatrix[16]{};
  float projMatrix3D[16]{};
  float rotAngle = 0.0f; // current Y-axis rotation (radians)

  // ── Effect quad pipeline (soft glow / beam / shockwave) ──────────────
  unsigned int effectVAO = 0, effectVBO = 0, effectEBO = 0;
  Shader effectShader;

  void initCubeMesh();
  void initPufferMesh();
  void initSpikeMesh();
  void initPufferFinMesh();
  void initPiranhaMesh();
  void initEffectQuad();

  void drawPufferfish(float worldX, float worldZ, float brightness);
  void drawPiranha(float worldX, float worldZ, bool horizontal,
                   float brightness);

  void buildPerspective(float fovDeg, float aspect, float near, float far);
  void buildViewMatrix();

  // Ray-casting helpers
  static void invertMatrix4(const float m[16], float inv[16]);
  void buildRay(double screenX, double screenY, float rayOrigin[3],
                float rayDir[3]) const;

  // 3D selection highlight
  void drawSelectionHighlight(float worldX, float worldZ);
  void drawEffectSprite(float worldX, float worldZ, float scaleX, float scaleZ,
                      float r, float g, float b, float a, int effectMode,
                      float progress, float yaw = 0.0f);
  void drawExplosionEffect(const ExplosionAnim &anim);
  void drawEffectBox(float worldX, float worldZ, float scaleX, float scaleY,
                     float scaleZ, float r, float g, float b, float a,
                     float yaw = 0.0f);
  void drawCube(unsigned int tex, float worldX, float worldZ, float r, float g,
                float b, float a);
};
