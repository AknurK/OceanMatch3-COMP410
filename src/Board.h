#pragma once
#include <random>
#include <utility>
#include <vector>

// ── Board dimensions ─────────────────────────────────────────────────────────
constexpr int BOARD_ROWS = 8;
constexpr int BOARD_COLS = 8;

// ── Tile types ───────────────────────────────────────────────────────────────
enum class TileType {
  NONE = -1,
  BLUE = 0,
  GREEN = 1,
  ORANGE = 2,
  PINK = 3,
  OBSTACLE_ICE = 4,
  OBSTACLE_WOOD = 5
};
constexpr int TILE_TYPE_COUNT = 4; // regular tile types only

// ------------------------------------------------------------
// Special tile types
// ------------------------------------------------------------
enum class SpecialType { NONE, PIRANHA_ROW, PIRANHA_COL, PUFFERFISH, COMBO_PUFFER_PIRANHA, COMBO_DOUBLE_PUFFER};

// ── Board cell ───────────────────────────────────────────────────────────────
struct Cell {
  TileType type = TileType::NONE;
  int hp = 0;
  int variant = 0; // 0-3: which of the 4 wood obstacle designs to use
  SpecialType special = SpecialType::NONE;

  bool isObstacle() const {
    return type == TileType::OBSTACLE_ICE || type == TileType::OBSTACLE_WOOD;
  }

  bool isEmpty() const { return type == TileType::NONE; }

  bool isRegularTile() const {
    return type == TileType::BLUE || type == TileType::GREEN ||
           type == TileType::ORANGE || type == TileType::PINK;
  }

  bool hasSpecial() const { return special != SpecialType::NONE; }
};

// ── Board ────────────────────────────────────────────────────────────────────
class Board {
public:
  int rows, cols;
  std::vector<std::vector<Cell>> grid;

  Board(int rows = BOARD_ROWS, int cols = BOARD_COLS);

  void randomFill();
  void placeObstacle(int r, int c, TileType obs, int variant = 0);

  // Match detection: returns all cell coords that form 3+ runs
  std::vector<std::pair<int, int>> findMatches() const;

  // Remove matched tiles, damage adjacent obstacles. Returns true if any.
  bool resolveMatches();
  int getLastClearedTileCount() const { return lastClearedTileCount; }
  void clearLastClearedTileCount() { lastClearedTileCount = 0; }

  // Gravity: tiles fall into empty cells. Returns true if anything moved.
  bool applyGravity();

  // Fill NONE cells with new random tiles (from top)
  void refill();

  // Swap adjacent tiles; reverts if no match created. Returns success.
  bool swapTiles(int r1, int c1, int r2, int c2);

  // Coordinate helper: grid (row, col) -> top-left pixel of tile in 2D space
  static std::pair<float, float> gridToWorld(int row, int col);

  // --------------------------------------------------------
  // Special explosion visualization support
  // --------------------------------------------------------
  const std::vector<std::pair<int, int>> &getLastSpecialAffectedCells() const {
    return lastSpecialAffectedCells;
  }

  SpecialType getLastSpecialType() const {
    return lastSpecialType;
  }

  std::pair<int, int> getLastSpecialOrigin() const {
    return {lastSpecialOriginR, lastSpecialOriginC};
  }

  void clearLastSpecialAffectedCells() {
    lastSpecialAffectedCells.clear();
    lastSpecialType = SpecialType::NONE;
    lastSpecialOriginR = -1;
    lastSpecialOriginC = -1;
  }

private:
  std::mt19937 rng;

  int lastSwapR1 = -1, lastSwapC1 = -1;
  int lastSwapR2 = -1, lastSwapC2 = -1;
  int lastClearedTileCount = 0;

  std::vector<std::pair<int, int>> lastSpecialAffectedCells;
  SpecialType lastSpecialType = SpecialType::NONE;
  int lastSpecialOriginR = -1;
  int lastSpecialOriginC = -1;

  TileType randomTile();
  bool inBounds(int r, int c) const;
  bool createsInitialMatch(int r, int c, TileType t) const;

  void damageObstacleAt(int r, int c);
  void clearCells(const std::vector<std::pair<int, int>> &cells);

  void triggerSpecialAt(int r, int c);

  //for combo pfish+piranha
  void triggerPufferPiranhaComboAt(int r, int c);

  //for combo of 2 pfish
  void triggerDoublePufferComboAt(int r, int c);

  std::vector<std::pair<int, int>>
  getSpecialEffectCells(int r, int c, SpecialType st) const;

  void applySpecialChainFromAffected(
      int originR, int originC, SpecialType visualType,
      const std::vector<std::pair<int, int>> &initialAffected);

  bool activatePreExistingSpecials(int r1, int c1, int r2, int c2,
                                   bool firstWasSpecial, bool secondWasSpecial);

  void damageAdjacentObstacles(const std::vector<std::pair<int, int>> &cells);
};
