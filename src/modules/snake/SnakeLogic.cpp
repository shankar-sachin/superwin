#include "modules/snake/SnakeLogic.h"

namespace superwin {

SnakeGame::SnakeGame(int cols, int rows, uint32_t seed)
    : cols_(cols < 5 ? 5 : cols), rows_(rows < 5 ? 5 : rows), rng_(seed ? seed : 1u) {
    Reset();
}

uint32_t SnakeGame::Next() {
    uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x;
    return x;
}

void SnakeGame::Reset() {
    body_.clear();
    const int cy = rows_ / 2;
    const int cx = cols_ / 2;
    // Three-segment snake centred, head to the right, moving right.
    body_.push_back({cx + 1, cy});
    body_.push_back({cx, cy});
    body_.push_back({cx - 1, cy});
    dir_ = pending_ = Dir::Right;
    score_ = 0;
    over_ = false;
    PlaceFood();
}

void SnakeGame::SetDirection(Dir d) {
    const bool reversal =
        (d == Dir::Up && dir_ == Dir::Down) || (d == Dir::Down && dir_ == Dir::Up) ||
        (d == Dir::Left && dir_ == Dir::Right) || (d == Dir::Right && dir_ == Dir::Left);
    if (!reversal) pending_ = d;
}

bool SnakeGame::Tick() {
    if (over_) return false;
    dir_ = pending_;

    Cell h = body_.front();
    switch (dir_) {
        case Dir::Up:    h.y -= 1; break;
        case Dir::Down:  h.y += 1; break;
        case Dir::Left:  h.x -= 1; break;
        case Dir::Right: h.x += 1; break;
    }

    if (h.x < 0 || h.x >= cols_ || h.y < 0 || h.y >= rows_) { over_ = true; return false; }

    const bool growing = (h == food_);
    // Self collision: the tail cell is vacated this tick unless we are growing.
    for (size_t i = 0; i < body_.size(); ++i) {
        if (i == body_.size() - 1 && !growing) continue;
        if (body_[i] == h) { over_ = true; return false; }
    }

    body_.push_front(h);
    if (growing) {
        ++score_;
        PlaceFood();
    } else {
        body_.pop_back();
    }
    return true;
}

void SnakeGame::PlaceFood() {
    const int total = cols_ * rows_;
    if (static_cast<int>(body_.size()) >= total) { over_ = true; return; }  // board full = win
    for (int attempt = 0; attempt < total * 4; ++attempt) {
        Cell c{static_cast<int>(Next() % static_cast<uint32_t>(cols_)),
               static_cast<int>(Next() % static_cast<uint32_t>(rows_))};
        bool onSnake = false;
        for (auto const& b : body_) if (b == c) { onSnake = true; break; }
        if (!onSnake) { food_ = c; return; }
    }
    // Fallback: first free cell scan (extremely unlikely to reach here).
    for (int y = 0; y < rows_; ++y)
        for (int x = 0; x < cols_; ++x) {
            Cell c{x, y};
            bool onSnake = false;
            for (auto const& b : body_) if (b == c) { onSnake = true; break; }
            if (!onSnake) { food_ = c; return; }
        }
}

}  // namespace superwin
