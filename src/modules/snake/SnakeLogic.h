// Snake game logic: a pure, deterministic state machine (grid, snake body, food,
// score, collisions) in superwin_core so it is unit-testable without any UI. The
// page only renders this and feeds it a direction + a timer tick.
#pragma once

#include <cstdint>
#include <deque>

namespace superwin {

enum class Dir { Up, Down, Left, Right };

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

class SnakeGame {
public:
    SnakeGame(int cols, int rows, uint32_t seed = 0x5eed1234u);

    void Reset();
    // Queue a turn. A 180° reversal into the snake's own neck is ignored.
    void SetDirection(Dir d);
    // Advance one step. Returns false (and sets gameOver) on a wall/self collision.
    bool Tick();

    bool gameOver() const { return over_; }
    int score() const { return score_; }
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    const std::deque<Cell>& body() const { return body_; }
    Cell food() const { return food_; }

    // Test hook: force the food to a known cell.
    void SetFoodForTest(Cell c) { food_ = c; }

private:
    void PlaceFood();

    int cols_, rows_;
    uint32_t rng_;
    std::deque<Cell> body_;
    Cell food_{};
    Dir dir_ = Dir::Right;
    Dir pending_ = Dir::Right;
    int score_ = 0;
    bool over_ = false;

    uint32_t Next();  // xorshift32
};

}  // namespace superwin
