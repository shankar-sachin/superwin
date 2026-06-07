#include <catch2/catch_test_macros.hpp>

#include "modules/snake/SnakeLogic.h"

using namespace superwin;

TEST_CASE("New game initial state", "[snake]") {
    SnakeGame g(20, 20);
    REQUIRE(g.score() == 0);
    REQUIRE_FALSE(g.gameOver());
    REQUIRE(g.body().size() == 3);
    REQUIRE(g.cols() == 20);
    REQUIRE(g.rows() == 20);
}

TEST_CASE("Moving advances the head", "[snake]") {
    SnakeGame g(20, 20);
    g.SetFoodForTest({0, 0});  // keep food out of the way
    const Cell before = g.body().front();
    REQUIRE(g.Tick());
    const Cell after = g.body().front();
    REQUIRE(after.x == before.x + 1);  // default direction is Right
    REQUIRE(after.y == before.y);
    REQUIRE(g.body().size() == 3);     // no growth without eating
}

TEST_CASE("Reversing into the neck is ignored", "[snake]") {
    SnakeGame g(20, 20);
    g.SetFoodForTest({0, 0});
    g.SetDirection(Dir::Left);  // opposite of Right -> ignored
    REQUIRE(g.Tick());
    // Still moved right, did not collide with its own neck.
    REQUIRE_FALSE(g.gameOver());
}

TEST_CASE("Hitting a wall ends the game", "[snake]") {
    SnakeGame g(8, 8);
    g.SetFoodForTest({0, 0});
    bool alive = true;
    for (int i = 0; i < 20 && alive; ++i) alive = g.Tick();  // run into the right wall
    REQUIRE(g.gameOver());
}

TEST_CASE("Eating food grows the snake and scores", "[snake]") {
    SnakeGame g(20, 20);
    const Cell head = g.body().front();
    g.SetFoodForTest({head.x + 1, head.y});  // food directly ahead
    REQUIRE(g.Tick());
    REQUIRE(g.score() == 1);
    REQUIRE(g.body().size() == 4);
}
