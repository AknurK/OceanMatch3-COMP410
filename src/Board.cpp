#include "Board.h"
#include <algorithm>
#include <ctime>
#include <set>
#include <queue>
#include <set>
#include <vector>

// ------------------------------------------------------------
// Helper structs
// ------------------------------------------------------------
namespace {
struct RunInfo {
  bool horizontal = true;
  TileType type = TileType::NONE;
  std::vector<std::pair<int, int>> cells;
};

struct SquareInfo {
  TileType type = TileType::NONE;
  std::vector<std::pair<int, int>> cells;
};

struct SpawnInfo {
  bool valid = false;
  int row = -1;
  int col = -1;
  TileType type = TileType::NONE;
  SpecialType special = SpecialType::NONE;
};
bool containsCell(const std::vector<std::pair<int, int>> &cells, int r, int c) {
  for (auto [rr, cc] : cells) {
    if (rr == r && cc == c)
      return true;
  }
  return false;
}

bool isPiranhaSpecial(SpecialType st) {
  return st == SpecialType::PIRANHA_ROW || st == SpecialType::PIRANHA_COL;
}
} // namespace


Board::Board(int r, int c)
    : rows(r), cols(c), grid(r, std::vector<Cell>(c)),
      rng(static_cast<unsigned>(std::time(nullptr))) {}

// ------------------------------------------------------------
// inBounds
// ------------------------------------------------------------
bool Board::inBounds(int r, int c) const {
  return r >= 0 && r < rows && c >= 0 && c < cols;
}

TileType Board::randomTile() {
  return static_cast<TileType>(rng() % TILE_TYPE_COUNT);
}

void Board::placeObstacle(int r, int c, TileType obs, int variant) {
  grid[r][c].type = obs;
  grid[r][c].hp = (obs == TileType::OBSTACLE_WOOD) ? 2 : 1;
  grid[r][c].variant = variant;
  grid[r][c].special = SpecialType::NONE;
}

// ------------------------------------------------------------
// createsInitialMatch
// ------------------------------------------------------------
bool Board::createsInitialMatch(int r, int c, TileType t) const {
  bool hMatch =
      (c >= 2 && grid[r][c - 1].type == t && grid[r][c - 2].type == t);

  bool vMatch =
      (r >= 2 && grid[r - 1][c].type == t && grid[r - 2][c].type == t);

  bool squareTL = (r >= 1 && c >= 1 && grid[r - 1][c].type == t &&
                   grid[r][c - 1].type == t && grid[r - 1][c - 1].type == t);

  bool squareTR = (r >= 1 && c + 1 < cols && grid[r - 1][c].type == t &&
                   grid[r][c + 1].type == t && grid[r - 1][c + 1].type == t);

  bool squareBL = (r + 1 < rows && c >= 1 && grid[r + 1][c].type == t &&
                   grid[r][c - 1].type == t && grid[r + 1][c - 1].type == t);

  bool squareBR = (r + 1 < rows && c + 1 < cols && grid[r + 1][c].type == t &&
                   grid[r][c + 1].type == t && grid[r + 1][c + 1].type == t);

  return hMatch || vMatch || squareTL || squareTR || squareBL || squareBR;
}

void Board::randomFill() {
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      if (!grid[r][c].isEmpty())
        continue; // keep pre-placed items
      TileType t;
      int attempts = 0;
      do {
        t = randomTile();
        attempts++;
        if (!createsInitialMatch(r, c, t))
          break;
      } while (attempts < 30);
      grid[r][c].type = t;
      grid[r][c].hp = 0;
      grid[r][c].variant = 0;
      grid[r][c].special = SpecialType::NONE;
    }
  }
}

std::vector<std::pair<int, int>> Board::findMatches() const {
  std::vector<std::vector<bool>> matched(rows, std::vector<bool>(cols, false));

  // Horizontal runs
  for (int r = 0; r < rows; r++) {
    int c = 0;
    while (c < cols) {
      TileType t = grid[r][c].type;
      if (t == TileType::NONE || grid[r][c].isObstacle()) {
        c++;
        continue;
      }
      int end = c + 1;
      while (end < cols && grid[r][end].type == t)
        end++;
      if (end - c >= 3)
        for (int k = c; k < end; k++)
          matched[r][k] = true;
      c = end;
    }
  }

  // Vertical runs
  for (int c = 0; c < cols; c++) {
    int r = 0;
    while (r < rows) {
      TileType t = grid[r][c].type;
      if (t == TileType::NONE || grid[r][c].isObstacle()) {
        r++;
        continue;
      }
      int end = r + 1;
      while (end < rows && grid[end][c].type == t)
        end++;
      if (end - r >= 3)
        for (int k = r; k < end; k++)
          matched[k][c] = true;
      r = end;
    }
  }
  // 2x2 square matches
  for (int r = 0; r < rows - 1; r++) {
    for (int c = 0; c < cols - 1; c++) {
      const Cell &a = grid[r][c];
      const Cell &b = grid[r][c + 1];
      const Cell &d = grid[r + 1][c];
      const Cell &e = grid[r + 1][c + 1];

      if (a.isObstacle() || b.isObstacle() || d.isObstacle() || e.isObstacle())
        continue;
      if (a.type == TileType::NONE)
        continue;

      if (a.type == b.type && a.type == d.type && a.type == e.type) {
        matched[r][c] = true;
        matched[r][c + 1] = true;
        matched[r + 1][c] = true;
        matched[r + 1][c + 1] = true;
      }
    }
  }

  std::vector<std::pair<int, int>> result;
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++)
      if (matched[r][c])
        result.push_back({r, c});
  return result;
}

// ------------------------------------------------------------
// damageObstacleAt
// ------------------------------------------------------------
void Board::damageObstacleAt(int r, int c) {
  if (!inBounds(r, c))
    return;

  Cell &cell = grid[r][c];
  if (!cell.isObstacle())
    return;

  cell.hp--;

  if (cell.hp <= 0) {
    cell = Cell{};
  }
}

// ------------------------------------------------------------
// damageAdjacentObstacles
// ------------------------------------------------------------
void Board::damageAdjacentObstacles(
    const std::vector<std::pair<int, int>> &cells) {
  const int dr[] = {-1, 1, 0, 0};
  const int dc[] = {0, 0, -1, 1};

  for (auto [r, c] : cells) {
    for (int d = 0; d < 4; d++) {
      int nr = r + dr[d];
      int nc = c + dc[d];
      damageObstacleAt(nr, nc);
    }
  }
}

void Board::clearCells(const std::vector<std::pair<int, int>> &cells) {
  std::set<std::pair<int, int>> uniqueCells(cells.begin(), cells.end());

  for (auto [r, c] : uniqueCells) {
    if (!inBounds(r, c))
      continue;

    Cell &cell = grid[r][c];

    if (cell.isObstacle()) {
      damageObstacleAt(r, c);
    } else {
      if (cell.type != TileType::NONE)
        lastClearedTileCount++;
      cell = Cell{};
    }
  }
}

std::vector<std::pair<int, int>>
Board::getSpecialEffectCells(int r, int c, SpecialType st) const {
  std::vector<std::pair<int, int>> affected;

  if (!inBounds(r, c))
    return affected;

  if (st == SpecialType::PIRANHA_ROW) {
    for (int cc = 0; cc < cols; cc++) {
      affected.push_back({r, cc});
    }
  } else if (st == SpecialType::PIRANHA_COL) {
    for (int rr = 0; rr < rows; rr++) {
      affected.push_back({rr, c});
    }
  } else if (st == SpecialType::PUFFERFISH) {
    for (int rr = r - 1; rr <= r + 1; rr++) {
      for (int cc = c - 1; cc <= c + 1; cc++) {
        if (inBounds(rr, cc)) {
          affected.push_back({rr, cc});
        }
      }
    }
  }

  return affected;
}

void Board::applySpecialChainFromAffected(
    int originR, int originC, SpecialType visualType,
    const std::vector<std::pair<int, int>> &initialAffected) {
  std::set<std::pair<int, int>> affectedSet;
  std::set<std::pair<int, int>> queuedSpecials;
  std::queue<std::pair<int, int>> specialQueue;

  auto addAffectedCell = [&](int rr, int cc) {
    if (!inBounds(rr, cc))
      return;

    affectedSet.insert({rr, cc});

    if (grid[rr][cc].hasSpecial()) {
      std::pair<int, int> pos = {rr, cc};

      if (queuedSpecials.find(pos) == queuedSpecials.end()) {
        queuedSpecials.insert(pos);
        specialQueue.push(pos);
      }
    }
  };

  // First add the original effect area.
  for (auto [rr, cc] : initialAffected) {
    addAffectedCell(rr, cc);
  }

  // Then process every special tile that is hit by the effect.
  // Since cells are not cleared until the end, all chained specials can still
  // be detected correctly from the grid.
  while (!specialQueue.empty()) {
    auto [sr, sc] = specialQueue.front();
    specialQueue.pop();

    if (!inBounds(sr, sc))
      continue;

    SpecialType chainedType = grid[sr][sc].special;
    if (chainedType == SpecialType::NONE)
      continue;

    std::vector<std::pair<int, int>> chainedAffected =
        getSpecialEffectCells(sr, sc, chainedType);

    for (auto [rr, cc] : chainedAffected) {
      addAffectedCell(rr, cc);
    }
  }

  std::vector<std::pair<int, int>> affected(affectedSet.begin(),
                                            affectedSet.end());

  lastSpecialAffectedCells = affected;
  lastSpecialType = visualType;
  lastSpecialOriginR = originR;
  lastSpecialOriginC = originC;

  clearCells(affected);
}
// ------------------------------------------------------------
// triggerSpecialAt
// ------------------------------------------------------------
void Board::triggerSpecialAt(int r, int c) {
  if (!inBounds(r, c))
    return;

  SpecialType st = grid[r][c].special;
  if (st == SpecialType::NONE)
    return;

  std::vector<std::pair<int, int>> initialAffected =
      getSpecialEffectCells(r, c, st);

  applySpecialChainFromAffected(r, c, st, initialAffected);
}
// ------------------------------------------------------------
// triggerPufferPiranhaComboAt
// ------------------------------------------------------------
void Board::triggerPufferPiranhaComboAt(int r, int c) {
  if (!inBounds(r, c))
    return;

  std::set<std::pair<int, int>> affectedSet;

  auto addCell = [&](int rr, int cc) {
    if (inBounds(rr, cc)) {
      affectedSet.insert({rr, cc});
    }
  };

  // Pufferfish + Piranha combo:
  // Direction-independent TNT + rocket behavior.
  // It clears 3 full rows and 3 full columns centered around the combo cell.

  // 1) Clear the full row above, center row, and row below.
  for (int rr = r - 1; rr <= r + 1; rr++) {
    for (int cc = 0; cc < cols; cc++) {
      addCell(rr, cc);
    }
  }

  // 2) Clear the full column left, center column, and right column.
  for (int cc = c - 1; cc <= c + 1; cc++) {
    for (int rr = 0; rr < rows; rr++) {
      addCell(rr, cc);
    }
  }

  std::vector<std::pair<int, int>> affected(affectedSet.begin(),
                                            affectedSet.end());

  // Keep the visual animation active. For now we reuse the Pufferfish effect
  // type, but the affected area is much larger than a normal 3x3 explosion.
  applySpecialChainFromAffected(r, c, SpecialType::COMBO_PUFFER_PIRANHA, affected);
}

// ------------------------------------------------------------
// triggerDoublePufferComboAt
// ------------------------------------------------------------
void Board::triggerDoublePufferComboAt(int r, int c) {
  if (!inBounds(r, c))
    return;

  std::set<std::pair<int, int>> affectedSet;

  auto addCell = [&](int rr, int cc) {
    if (inBounds(rr, cc)) {
      affectedSet.insert({rr, cc});
    }
  };

  // Pufferfish + Pufferfish combo:
  // TNT + TNT style behavior. It clears a larger 6x6 area around the
  // combo center.
  for (int rr = r - 2; rr <= r + 3; rr++) {
    for (int cc = c - 2; cc <= c + 3; cc++) {
      addCell(rr, cc);
    }
  }

  std::vector<std::pair<int, int>> affected(affectedSet.begin(),
                                            affectedSet.end());
  applySpecialChainFromAffected(r, c, SpecialType::COMBO_DOUBLE_PUFFER, affected);
}

// ------------------------------------------------------------
// activatePreExistingSpecials
// ------------------------------------------------------------
bool Board::activatePreExistingSpecials(int r1, int c1, int r2, int c2,
                                        bool firstWasSpecial,
                                        bool secondWasSpecial) {
  // After the swap:
  // - the first selected tile is now at (r2, c2)
  // - the second selected tile is now at (r1, c1)
  if (firstWasSpecial && secondWasSpecial) {
  SpecialType firstSpecial = grid[r2][c2].special;
  SpecialType secondSpecial = grid[r1][c1].special;

  bool firstIsPuffer = firstSpecial == SpecialType::PUFFERFISH;
  bool secondIsPuffer = secondSpecial == SpecialType::PUFFERFISH;
  bool firstIsPiranha =
      firstSpecial == SpecialType::PIRANHA_ROW ||
      firstSpecial == SpecialType::PIRANHA_COL;
  bool secondIsPiranha =
      secondSpecial == SpecialType::PIRANHA_ROW ||
      secondSpecial == SpecialType::PIRANHA_COL;

  // Pufferfish + Pufferfish combo:
  // Larger TNT + TNT style blast.
  if (firstIsPuffer && secondIsPuffer) {
    triggerDoublePufferComboAt(r2, c2);
    return true;
  }

  // Pufferfish + Piranha combo:
  // Ignore the Piranha direction. The combo always clears both directions:
  // 3 full rows + 3 full columns.
  if ((firstIsPuffer && secondIsPiranha) ||
      (secondIsPuffer && firstIsPiranha)) {
    if (firstIsPiranha) {
      triggerPufferPiranhaComboAt(r2, c2);
    } else {
      triggerPufferPiranhaComboAt(r1, c1);
    }
    return true;
  }
}

  bool activated = false;

  if (firstWasSpecial) {
    triggerSpecialAt(r2, c2);
    activated = true;
  }

  if (secondWasSpecial) {
    triggerSpecialAt(r1, c1);
    activated = true;
  }

  return activated;
}

// ------------------------------------------------------------
// resolveMatches
// ------------------------------------------------------------
bool Board::resolveMatches() {
  lastClearedTileCount = 0;

  std::vector<RunInfo> runs;
  std::vector<SquareInfo> squares;
  std::vector<std::vector<bool>> matched(rows, std::vector<bool>(cols, false));

  // Horizontal runs
  for (int r = 0; r < rows; r++) {
    int c = 0;
    while (c < cols) {
      TileType t = grid[r][c].type;

      if (t == TileType::NONE || grid[r][c].isObstacle()) {
        c++;
        continue;
      }

      int end = c + 1;
      while (end < cols && grid[r][end].type == t &&
             !grid[r][end].isObstacle()) {
        end++;
      }

      int len = end - c;
      if (len >= 3) {
        RunInfo run;
        run.horizontal = true;
        run.type = t;
        for (int k = c; k < end; k++) {
          matched[r][k] = true;
          run.cells.push_back({r, k});
        }
        runs.push_back(run);
      }

      c = end;
    }
  }

  // Vertical runs
  for (int c = 0; c < cols; c++) {
    int r = 0;
    while (r < rows) {
      TileType t = grid[r][c].type;

      if (t == TileType::NONE || grid[r][c].isObstacle()) {
        r++;
        continue;
      }

      int end = r + 1;
      while (end < rows && grid[end][c].type == t &&
             !grid[end][c].isObstacle()) {
        end++;
      }

      int len = end - r;
      if (len >= 3) {
        RunInfo run;
        run.horizontal = false;
        run.type = t;
        for (int k = r; k < end; k++) {
          matched[k][c] = true;
          run.cells.push_back({k, c});
        }
        runs.push_back(run);
      }

      r = end;
    }
  }

  // 2x2 square matches
  for (int r = 0; r < rows - 1; r++) {
    for (int c = 0; c < cols - 1; c++) {
      const Cell &a = grid[r][c];
      const Cell &b = grid[r][c + 1];
      const Cell &d = grid[r + 1][c];
      const Cell &e = grid[r + 1][c + 1];

      if (a.isObstacle() || b.isObstacle() || d.isObstacle() || e.isObstacle())
        continue;
      if (a.type == TileType::NONE)
        continue;

      if (a.type == b.type && a.type == d.type && a.type == e.type) {

        matched[r][c] = true;
        matched[r][c + 1] = true;
        matched[r + 1][c] = true;
        matched[r + 1][c + 1] = true;

        SquareInfo sq;
        sq.type = a.type;
        sq.cells.push_back({r, c});
        sq.cells.push_back({r, c + 1});
        sq.cells.push_back({r + 1, c});
        sq.cells.push_back({r + 1, c + 1});
        squares.push_back(sq);
      }
    }
  }

  std::vector<std::pair<int, int>> allMatched;
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      if (matched[r][c])
        allMatched.push_back({r, c});
    }
  }

  if (allMatched.empty())
    return false;

  SpawnInfo spawn;

  // 1) Prefer 5+ runs -> PUFFERFISH
  for (const RunInfo &run : runs) {
    if ((int)run.cells.size() >= 5) {
      int sr = run.cells.front().first;
      int sc = run.cells.front().second;

      for (auto [rr, cc] : run.cells) {
        if ((rr == lastSwapR1 && cc == lastSwapC1) ||
            (rr == lastSwapR2 && cc == lastSwapC2)) {
          sr = rr;
          sc = cc;
          break;
        }
      }

      spawn.valid = true;
      spawn.row = sr;
      spawn.col = sc;
      spawn.type = run.type;
      spawn.special = SpecialType::PUFFERFISH;
      break;
    }
  }

  // 2) 2x2 square -> PUFFERFISH
  if (!spawn.valid) {
    for (const SquareInfo &sq : squares) {
      int sr = sq.cells[0].first;
      int sc = sq.cells[0].second;

      for (auto [rr, cc] : sq.cells) {
        if ((rr == lastSwapR1 && cc == lastSwapC1) ||
            (rr == lastSwapR2 && cc == lastSwapC2)) {
          sr = rr;
          sc = cc;
          break;
        }
      }

      spawn.valid = true;
      spawn.row = sr;
      spawn.col = sc;
      spawn.type = sq.type;
      spawn.special = SpecialType::PUFFERFISH;
      break;
    }
  }
// 3) 4-line run -> directional piranha
// The Piranha direction is determined by the direction of the swap that
// created it, not only by the orientation of the detected 4-match.
// Horizontal swap  -> row-clearing Piranha
// Vertical swap    -> column-clearing Piranha
if (!spawn.valid) {
  const RunInfo *chosenRun = nullptr;
  int sr = -1;
  int sc = -1;

  bool hasValidLastSwap =
      lastSwapR1 >= 0 && lastSwapC1 >= 0 && lastSwapR2 >= 0 &&
      lastSwapC2 >= 0;

  bool lastSwapWasHorizontal =
      hasValidLastSwap && lastSwapR1 == lastSwapR2 &&
      std::abs(lastSwapC1 - lastSwapC2) == 1;

  bool lastSwapWasVertical =
      hasValidLastSwap && lastSwapC1 == lastSwapC2 &&
      std::abs(lastSwapR1 - lastSwapR2) == 1;

  auto runContainsCell = [](const RunInfo &run, int r, int c) {
    for (auto [rr, cc] : run.cells) {
      if (rr == r && cc == c)
        return true;
    }
    return false;
  };

  // First prefer a 4-run that contains one of the cells involved in the swap.
  // This makes the created Piranha belong to the player's actual move.
  if (hasValidLastSwap) {
    for (const RunInfo &run : runs) {
      if ((int)run.cells.size() == 4 &&
          runContainsCell(run, lastSwapR2, lastSwapC2)) {
        chosenRun = &run;
        sr = lastSwapR2;
        sc = lastSwapC2;
        break;
      }
    }

    if (!chosenRun) {
      for (const RunInfo &run : runs) {
        if ((int)run.cells.size() == 4 &&
            runContainsCell(run, lastSwapR1, lastSwapC1)) {
          chosenRun = &run;
          sr = lastSwapR1;
          sc = lastSwapC1;
          break;
        }
      }
    }
  }

  // Fallback for cascades or automatic matches.
  if (!chosenRun) {
    for (const RunInfo &run : runs) {
      if ((int)run.cells.size() == 4) {
        chosenRun = &run;
        sr = run.cells.front().first;
        sc = run.cells.front().second;
        break;
      }
    }
  }

  if (chosenRun) {
    spawn.valid = true;
    spawn.row = sr;
    spawn.col = sc;
    spawn.type = chosenRun->type;

    if (lastSwapWasHorizontal) {
      spawn.special = SpecialType::PIRANHA_ROW;
    } else if (lastSwapWasVertical) {
      spawn.special = SpecialType::PIRANHA_COL;
    } else {
      // Cascade fallback: if no direct player swap created this 4-match,
      // use the detected run orientation.
      spawn.special = chosenRun->horizontal ? SpecialType::PIRANHA_ROW
                                            : SpecialType::PIRANHA_COL;
    }
  }
}


  damageAdjacentObstacles(allMatched);

  for (auto [r, c] : allMatched) {
    if (spawn.valid && r == spawn.row && c == spawn.col)
      continue;
    Cell &cell = grid[r][c];
    if (cell.type != TileType::NONE && !cell.isObstacle())
      lastClearedTileCount++;
    grid[r][c] = Cell{};
  }

  if (spawn.valid && inBounds(spawn.row, spawn.col)) {
    grid[spawn.row][spawn.col].type = spawn.type;
    grid[spawn.row][spawn.col].hp = 0;
    grid[spawn.row][spawn.col].variant = 0;
    grid[spawn.row][spawn.col].special = spawn.special;
  }

  lastSwapR1 = lastSwapC1 = lastSwapR2 = lastSwapC2 = -1;
  return true;
}

bool Board::applyGravity() {
  bool moved = false;
  for (int c = 0; c < cols; c++) {
    for (int r = rows - 1; r >= 0; r--) {
      if (grid[r][c].type != TileType::NONE)
        continue;
      // Find tile above that isn't an obstacle
      for (int above = r - 1; above >= 0; above--) {
        if (grid[above][c].isObstacle())
          break; // obstacles block gravity
        if (grid[above][c].type != TileType::NONE) {
          grid[r][c] = grid[above][c];
          grid[above][c] = Cell{};
          moved = true;
          break;
        }
      }
    }
  }
  return moved;
}

void Board::refill() {
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++)
      if (grid[r][c].type == TileType::NONE)
        grid[r][c].type = randomTile();
}



// ------------------------------------------------------------
// swapTiles
// ------------------------------------------------------------
bool Board::swapTiles(int r1, int c1, int r2, int c2) {
  lastClearedTileCount = 0;

  if (!inBounds(r1, c1) || !inBounds(r2, c2))
    return false;
  if (std::abs(r2 - r1) + std::abs(c2 - c1) != 1)
    return false;
  if (grid[r1][c1].isObstacle() || grid[r2][c2].isObstacle())
    return false;
  if (grid[r1][c1].type == TileType::NONE ||
      grid[r2][c2].type == TileType::NONE)
    return false;

  clearLastSpecialAffectedCells();

  bool firstWasSpecial = grid[r1][c1].hasSpecial();
  bool secondWasSpecial = grid[r2][c2].hasSpecial();

  std::swap(grid[r1][c1], grid[r2][c2]);

  if (activatePreExistingSpecials(r1, c1, r2, c2, firstWasSpecial,
                                  secondWasSpecial)) {
    return true;
  }

  if (findMatches().empty()) {
    std::swap(grid[r1][c1], grid[r2][c2]);
    return false;
  }

  lastSwapR1 = r1;
  lastSwapC1 = c1;
  lastSwapR2 = r2;
  lastSwapC2 = c2;
  return true;
}

// board offset = 90, cellSize = 90, padding = 5
std::pair<float, float> Board::gridToWorld(int row, int col) {
  float x = 90.0f + col * 90.0f + 5.0f;
  float y = 90.0f + row * 90.0f + 5.0f;
  return {x, y};
}
