#include "Game.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using compintel::Game;
using compintel::GameKind;
using compintel::Move;
using compintel::Position;

namespace
{
void Require(bool condition, std::string const& message)
{
    if (!condition) throw std::runtime_error(message);
}

void Play(Game& game, Move move)
{
    std::string error;
    if (!game.Play(move, &error))
        throw std::runtime_error("Unexpected illegal move " + game.FormatMove(move) + ": " + error);
}

bool Contains(std::vector<Move> const& moves, int x, int y)
{
    return std::any_of(moves.begin(), moves.end(), [=](Move move) { return move.x == x && move.y == y; });
}

Game FacingPawns()
{
    Game game(GameKind::Quoridor);
    for (int y = 1; y <= 4; ++y)
    {
        Play(game, {4, y});
        Play(game, {y % 2 ? 3 : 4, 8});
    }
    Play(game, {4, 5});
    Play(game, {4, 7});
    Play(game, {1, 1, true, 0});
    Play(game, {4, 6});
    return game;
}

void ProtocolRoundTrip()
{
    Game gomoku;
    Require(gomoku.Initialization(0) == "1\n15 15\n0\n", "Gomoku initialization must match protocol v1");
    Require(gomoku.Initialization(1) == "1\n15 15\n1\n", "Gomoku second seat initialization");
    Move parsed;
    Require(gomoku.ParseMove(" \t7 8\r\n", parsed) && parsed == Move{7, 8}, "Whitespace parsing");
    Require(gomoku.FormatMove(parsed) == "7 8\n", "Gomoku output formatting");
    for (auto output : {"7 8 debug", "15 0", "1e0 2", "-1 2", "+1 2", "01 2", "1 2 3", "", "999999999999999999999999999 2", "1.0 2"})
        Require(!gomoku.ParseMove(output, parsed), std::string("Reject malformed Gomoku output: ") + output);

    Game quoridor(GameKind::Quoridor);
    Require(quoridor.Initialization(1) == "1 1\n", "Quoridor initialization");
    for (Move move : {Move{4, 1}, Move{3, 7, true, 0}, Move{8, 8, true, 1}})
        Require(quoridor.ParseMove(quoridor.FormatMove(move), parsed) && parsed == move, "Quoridor round trip");
    for (auto output : {"0 4", "0 4 1 debug", "1 4 1 2", "0 -1 2", "1 0 1 1", "0 4 9", "2 4 1", "0 04 1", "1 4 1 0 0"})
        Require(!quoridor.ParseMove(output, parsed), std::string("Reject malformed Quoridor output: ") + output);
}

void GomokuWinDirections()
{
    for (auto direction : {Position{1, 0}, Position{0, 1}, Position{1, 1}, Position{1, -1}})
    {
        Game game;
        for (int n = 0; n < 5; ++n)
        {
            Play(game, {3 + direction.x * n, 7 + direction.y * n});
            if (n < 4) Play(game, {n * 2, 14});
        }
        Require(game.Finished() && game.Winner() == 0 && game.Result() == "win", "Five wins in all four directions");
        Require(game.FinishReason() == "five_in_a_row", "Gomoku win reason retained for UI and replay");
        Require(!game.Play({14, 0}), "No moves after a win");
        Require(game.Undo() && !game.Finished() && game.NextSeat() == 0, "Undo reopens terminal game");
        Require(game.At(3 + direction.x * 4, 7 + direction.y * 4) == -1, "Undo clears last stone");
    }
    Game game;
    Require(!game.Play({-1, 0}), "Out of bounds rejected");
    Require(!game.Play({0, 15}), "Bottom outside rejected");
    Play(game, {7, 7});
    Require(!game.Play({7, 7}), "Occupied intersection rejected");
    Require(!game.Play({8, 8, true}), "Gomoku wall rejected");
    Require(game.History().size() == 1 && game.NextSeat() == 1, "Rejected moves preserve state");
}

void GomokuOverlineAndDraw()
{
    Game overline;
    for (int x : {0, 1, 2, 4, 5})
    {
        Play(overline, {x, 5});
        Play(overline, {x * 2, 14});
    }
    Play(overline, {3, 5});
    Require(overline.Winner() == 0, "Six in a row counts as a win (no forbidden moves)");

    // The 0011 stripe coloring has no monochromatic run of five horizontally,
    // vertically or diagonally. Play by seat to exercise a truly full-board draw.
    Game draw;
    std::array<std::vector<Move>, 2> cells;
    for (int y = 0; y < 15; ++y)
        for (int x = 0; x < 15; ++x)
            cells[static_cast<size_t>((x + 2 * y) % 4 < 2 ? 0 : 1)].push_back({x, y});
    Require(cells[0].size() == 113 && cells[1].size() == 112, "Draw fixture seat counts");
    for (size_t i = 0; i < cells[0].size(); ++i)
    {
        Play(draw, cells[0][i]);
        if (i < cells[1].size()) Play(draw, cells[1][i]);
    }
    Require(draw.Result() == "draw" && draw.Winner() == -1, "Full board draw recognized");
    Require(draw.FinishReason() == "board_full", "Full board finish reason");
}

void GomokuBotTactics()
{
    Game opening;
    Require(opening.SuggestMove() == Move{7, 7}, "Opening prefers center");
    Game attack;
    for (int x = 0; x < 4; ++x)
    {
        Play(attack, {x, 3});
        Play(attack, {x * 2, 8});
    }
    auto win = attack.SuggestMove();
    Require(win == Move{4, 3}, "Bot plays immediate win");
    Play(attack, win);
    Require(attack.Winner() == 0, "Suggested winning move wins");

    Game defend;
    for (int x = 0; x < 4; ++x)
    {
        Play(defend, {x * 2, 8});
        Play(defend, {x, 3});
    }
    Require(defend.SuggestMove() == Move{4, 3}, "Bot blocks immediate opponent win");
}

void QuoridorMovesAndJumps()
{
    Game start(GameKind::Quoridor);
    Require(start.Pawn(0) == Position{4, 0} && start.Pawn(1) == Position{4, 8}, "Initial pawn positions");
    Require(start.WallsRemaining(0) == 10 && start.WallsRemaining(1) == 10, "Initial wall counts");
    Require(start.LegalPawnMoves(0).size() == 3, "Only three opening moves at edge");
    Require(!start.Play({4, 2}), "Cannot move two spaces without opponent");
    Require(!start.Play({5, 1}), "Cannot move diagonally normally");
    Play(start, {4, 1});
    Require(start.Pawn(0) == Position{4, 1} && start.NextSeat() == 1, "Ordinary movement and turn switch");

    auto jump = FacingPawns();
    auto moves = jump.LegalPawnMoves(0);
    Require(Contains(moves, 4, 7), "Straight jump over opponent");
    Require(!Contains(moves, 3, 6) && !Contains(moves, 5, 6), "No diagonal jump when straight jump is possible");
    Require(!Contains(moves, 4, 6), "Cannot occupy opponent square");
    Play(jump, {4, 7});
    Require(jump.Pawn(0) == Position{4, 7}, "Apply straight jump");

    auto diagonal = FacingPawns();
    Play(diagonal, {4, 7, true, 1});
    Play(diagonal, {8, 1, true, 0});
    moves = diagonal.LegalPawnMoves(0);
    Require(Contains(moves, 3, 6) && Contains(moves, 5, 6), "Side jumps allowed when blocked behind opponent");
    Require(!Contains(moves, 4, 7) && !Contains(moves, 4, 6), "Blocked straight jump and occupied square rejected");
    Play(diagonal, {3, 6});
    Require(diagonal.Pawn(0) == Position{3, 6}, "Apply diagonal jump");

    auto separated = FacingPawns();
    Play(separated, {4, 6, true, 1});
    Play(separated, {8, 1, true, 0});
    moves = separated.LegalPawnMoves(0);
    Require(!Contains(moves, 4, 7) && !Contains(moves, 3, 6) && !Contains(moves, 5, 6),
            "Cannot use opponent for a jump across a separating wall");

    Game edge(GameKind::Quoridor);
    for (int y = 1; y <= 7; ++y)
    {
        Play(edge, {4, y});
        if (y < 7) Play(edge, {y % 2 ? 3 : 4, 8});
        else Play(edge, {1, 1, true, 0});
    }
    moves = edge.LegalPawnMoves(0);
    Require(Contains(moves, 3, 8) && Contains(moves, 5, 8), "Board edge behind opponent permits side jumps");
}

void QuoridorWallConflictsAndUndo()
{
    Game game(GameKind::Quoridor);
    Play(game, {4, 1, true, 1});
    Require(game.WallsRemaining(0) == 9 && game.Walls().size() == 1, "Placing wall consumes one wall");
    Play(game, {4, 7});
    Require(!Contains(game.LegalPawnMoves(0), 4, 1), "Horizontal wall blocks downward movement");
    for (Move wall : {Move{3, 1, true, 1}, Move{4, 1, true, 1}, Move{5, 1, true, 1}, Move{4, 1, true, 0}})
        Require(!game.Play(wall), "Overlapping or crossing walls rejected");
    Require(game.History().size() == 2 && game.WallsRemaining(0) == 9, "Invalid wall placement preserves state");
    Play(game, {6, 1, true, 1});
    Require(game.Walls().size() == 2, "Walls can share endpoints");
    Require(game.Undo() && game.Walls().size() == 1 && game.WallsRemaining(0) == 9, "Undo restores wall list and supply");
    Require(game.Undo() && game.Pawn(1) == Position{4, 8}, "Undo restores pawn position");
    Require(game.Undo() && game.Walls().empty() && game.WallsRemaining(0) == 10, "Undo opening wall");
    Require(!game.Undo(), "Undo empty history returns false");

    Game exhausted(GameKind::Quoridor);
    for (int turn = 0; turn < 20; ++turn)
    {
        bool placed = false;
        for (int orientation = 0; orientation < 2 && !placed; ++orientation)
            for (int y = 1; y <= 8 && !placed; ++y)
                for (int x = 1; x <= 8 && !placed; ++x)
                {
                    Move wall{x, y, true, orientation};
                    if (exhausted.IsLegal(wall))
                    {
                        Play(exhausted, wall);
                        placed = true;
                    }
                }
        Require(placed, "Wall supply fixture can place all ten walls per seat");
    }
    Require(exhausted.WallsRemaining(0) == 0 && exhausted.WallsRemaining(1) == 0, "Both supplies can be exhausted");
    std::string error;
    Require(!exhausted.Play({8, 8, true, 0}, &error) && error == "No walls remaining.", "Cannot place an eleventh wall");
    Require(exhausted.IsLegal(exhausted.SuggestMove()), "Pawn movement remains available with no walls");
}

void QuoridorPathProtection()
{
    Game game(GameKind::Quoridor);
    std::vector<Move> walls{{7, 6, true, 0}, {1, 1, true, 0}, {3, 8, true, 1}, {1, 5, true, 0},
                            {8, 7, true, 1}, {3, 3, true, 1}, {4, 1, true, 0}, {3, 7, true, 1},
                            {6, 1, true, 1}, {4, 5, true, 0}, {2, 4, true, 0}, {5, 7, true, 1},
                            {8, 2, true, 0}, {3, 2, true, 1}, {5, 8, true, 0}, {1, 8, true, 1},
                            {5, 3, true, 1}};
    for (auto wall : walls) Play(game, wall);
    std::string error;
    Require(!game.Play({4, 7, true, 0}, &error), "Wall sealing final route rejected");
    Require(error.find("every path") != std::string::npos, "Path protection explains rejected move");
    Require(game.Walls().size() == walls.size() && game.History().size() == walls.size(), "Path rejection leaves state unchanged");
}

void QuoridorGoalAndMoveLimit()
{
    Game goal(GameKind::Quoridor);
    for (int y = 1; y <= 8; ++y)
    {
        Play(goal, {4, y});
        if (y < 8) Play(goal, {y % 2 ? 3 : 4, 8});
    }
    Require(goal.Result() == "win" && goal.Winner() == 0, "Reaching opposite edge wins");
    Require(goal.FinishReason() == "goal_reached", "Quoridor win reason differs from Gomoku");
    Require(goal.Undo() && !goal.Finished(), "Undo goal win");

    Game limited(GameKind::Quoridor);
    for (int turn = 0; turn < 200; ++turn)
    {
        int seat = limited.NextSeat();
        auto pawn = limited.Pawn(seat);
        Play(limited, {4, seat == 0 ? (pawn.y == 0 ? 1 : 0) : (pawn.y == 8 ? 7 : 8)});
        if (turn < 199) Require(!limited.Finished(), "Move limit must not trigger early");
    }
    Require(limited.Result() == "move_limit" && limited.Winner() == -1, "200-ply move limit recognized");
    Require(limited.FinishReason() == "move_limit", "Quoridor move limit reason");
    Require(limited.Undo() && !limited.Finished() && limited.NextSeat() == 1, "Undo restores state before move limit");
}

void BotsCompleteLegalGames()
{
    for (auto kind : {GameKind::Gomoku, GameKind::Quoridor})
    {
        Game game(kind);
        while (!game.Finished())
        {
            auto suggestion = game.SuggestMove();
            Require(game.IsLegal(suggestion), "Every bot suggestion must be legal");
            Require(suggestion == game.SuggestMove(), "Bot is deterministic");
            Play(game, suggestion);
            Require(game.History().size() <= 225, "Bot game terminates");
        }
        Require(game.Result() == "win" || game.Result() == "draw", "Built-in bots complete normally");
    }
}

void ForfeitState()
{
    Game game;
    game.Forfeit(1, "Time limit exceeded");
    Require(game.Finished() && game.Winner() == 0 && game.Result() == "forfeit", "Runtime error can forfeit seat");
    Require(game.FinishReason() == "Time limit exceeded", "Forfeit reason retained");
    Require(game.Undo() && !game.Finished(), "Can reset initial forfeit");
    game.Forfeit(-1, "invalid seat");
    Require(!game.Finished(), "Invalid forfeit seat rejected");
}
}

int main()
{
    std::vector<std::pair<char const*, std::function<void()>>> tests{
        {"protocol v1 round trips and malformed output", ProtocolRoundTrip},
        {"Gomoku win directions and invalid state preservation", GomokuWinDirections},
        {"Gomoku overline and full-board draw", GomokuOverlineAndDraw},
        {"Gomoku deterministic bot tactics", GomokuBotTactics},
        {"Quoridor movement, direct jumps and diagonal jumps", QuoridorMovesAndJumps},
        {"Quoridor wall conflicts and undo", QuoridorWallConflictsAndUndo},
        {"Quoridor final route protection", QuoridorPathProtection},
        {"Quoridor goal and 200-ply move limit", QuoridorGoalAndMoveLimit},
        {"Bots complete legal games", BotsCompleteLegalGames},
        {"Runtime forfeits", ForfeitState},
    };
    int failures = 0;
    for (auto const& [name, test] : tests)
    {
        try
        {
            test();
            std::cout << "PASS " << name << '\n';
        }
        catch (std::exception const& error)
        {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - static_cast<size_t>(failures) << '/' << tests.size() << " test groups passed\n";
    return failures == 0 ? 0 : 1;
}
